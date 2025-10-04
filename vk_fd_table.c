#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "vk_fd_table.h"
#include "vk_debug.h"
#include "vk_fd.h"
#include "vk_fd_table_s.h"
#include "vk_io_future.h"
#include "vk_io_exec.h"
#include "vk_io_batch_iface.h"
#include "vk_io_batch_libaio.h"
#include "vk_io_batch_libaio_s.h"
#if VK_USE_IO_URING
#include "vk_io_batch_uring.h"
#define VK_IO_URING_QUEUE_DEPTH 256
#endif
#include "vk_io_op.h"
#include "vk_io_queue.h"
#include "vk_kern.h"
#include "vk_proc.h"
#include "vk_socket.h"
#include "vk_thread_io.h"
#include "vk_sys_caps.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void vk_fd_table_ensure_caps(struct vk_fd_table* fd_table_ptr);
static void vk_fd_table_select_driver(struct vk_fd_table* fd_table_ptr);
static void vk_fd_table_switch_to_poll(struct vk_fd_table* fd_table_ptr);
static void vk_fd_table_os_deregister_fd(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr);
#if VK_USE_IO_URING
static int vk_fd_table_uring_setup(struct vk_fd_table* fd_table_ptr);
static void vk_fd_table_uring_teardown(struct vk_fd_table* fd_table_ptr);
static int vk_fd_table_uring(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr);
#endif

/*
 * Object Manipulation
 */
size_t vk_fd_table_object_size() { return sizeof (struct vk_fd_table); }
size_t vk_fd_table_entry_size() { return sizeof (struct vk_fd); }
size_t vk_fd_table_alloc_size(size_t size) { return sizeof(struct vk_fd_table) + (sizeof(struct vk_fd) * size); }

enum vk_poll_driver vk_fd_table_get_poll_driver(struct vk_fd_table* fd_table_ptr)
{
	if (!fd_table_ptr) {
		return VK_POLL_DRIVER_POLL;
	}
	vk_fd_table_select_driver(fd_table_ptr);
	return fd_table_ptr->poll_driver;
}
void vk_fd_table_set_poll_driver(struct vk_fd_table* fd_table_ptr, enum vk_poll_driver poll_driver)
{
	if (!fd_table_ptr) {
		return;
	}
	fd_table_ptr->poll_driver_requested = poll_driver;
	fd_table_ptr->poll_driver = poll_driver;
#if VK_USE_KQUEUE || VK_USE_EPOLL || VK_USE_GETEVENTS || VK_USE_IO_URING
	if (poll_driver != VK_POLL_DRIVER_OS) {
		fd_table_ptr->os_driver_kind = VK_OS_DRIVER_NONE;
#if VK_USE_IO_URING
		vk_fd_table_uring_teardown(fd_table_ptr);
#endif
	}
#endif
	if (poll_driver == VK_POLL_DRIVER_OS) {
		vk_fd_table_select_driver(fd_table_ptr);
	}
}

enum vk_poll_method vk_fd_table_get_poll_method(struct vk_fd_table* fd_table_ptr) { return fd_table_ptr->poll_method; }
void vk_fd_table_set_poll_method(struct vk_fd_table* fd_table_ptr, enum vk_poll_method poll_method)
{
	fd_table_ptr->poll_method = poll_method;
}

int
vk_fd_table_using_getevents(struct vk_fd_table* fd_table_ptr)
{
#if VK_USE_GETEVENTS
	vk_fd_table_select_driver(fd_table_ptr);
	return fd_table_ptr && fd_table_ptr->poll_driver == VK_POLL_DRIVER_OS &&
	       fd_table_ptr->os_driver_kind == VK_OS_DRIVER_GETEVENTS;
#else
	(void)fd_table_ptr;
	return 0;
#endif
}

int
vk_fd_table_using_io_uring(struct vk_fd_table* fd_table_ptr)
{
#if VK_USE_IO_URING
	vk_fd_table_select_driver(fd_table_ptr);
	return fd_table_ptr && fd_table_ptr->poll_driver == VK_POLL_DRIVER_OS &&
	       fd_table_ptr->os_driver_kind == VK_OS_DRIVER_IO_URING;
#else
	(void)fd_table_ptr;
	return 0;
#endif
}

static void
vk_fd_table_ensure_caps(struct vk_fd_table* fd_table_ptr)
{
#if VK_USE_KQUEUE || VK_USE_EPOLL || VK_USE_GETEVENTS || VK_USE_IO_URING
	if (!fd_table_ptr->sys_caps_initialized) {
		if (vk_sys_caps_detect(&fd_table_ptr->sys_caps) == -1) {
			/* leave caps zeroed; fallback logic will choose portable drivers */
		}
		fd_table_ptr->sys_caps_initialized = 1;
	}
#else
	(void)fd_table_ptr;
#endif
}

static void
vk_fd_table_select_driver(struct vk_fd_table* fd_table_ptr)
{
	if (!fd_table_ptr) {
		return;
	}

	enum vk_poll_driver desired = fd_table_ptr->poll_driver_requested;
	if (desired != VK_POLL_DRIVER_OS) {
        fd_table_ptr->poll_driver = desired;
#if VK_USE_KQUEUE || VK_USE_EPOLL || VK_USE_GETEVENTS || VK_USE_IO_URING
        fd_table_ptr->os_driver_kind = VK_OS_DRIVER_NONE;
#endif
        return;
    }

#if VK_USE_KQUEUE || VK_USE_EPOLL || VK_USE_GETEVENTS || VK_USE_IO_URING
	vk_fd_table_ensure_caps(fd_table_ptr);

#if VK_USE_IO_URING
	if (vk_sys_caps_have_io_uring(&fd_table_ptr->sys_caps)) {
		if (!fd_table_ptr->uring_initialized) {
			if (vk_fd_table_uring_setup(fd_table_ptr) == -1) {
				vk_fd_table_uring_teardown(fd_table_ptr);
			}
		}
		if (fd_table_ptr->uring_initialized) {
			fd_table_ptr->poll_driver = VK_POLL_DRIVER_OS;
			fd_table_ptr->os_driver_kind = VK_OS_DRIVER_IO_URING;
			return;
		}
	}
#endif

#if VK_USE_GETEVENTS
	if (vk_sys_caps_have_aio_poll(&fd_table_ptr->sys_caps)) {
		fd_table_ptr->poll_driver = VK_POLL_DRIVER_OS;
		fd_table_ptr->os_driver_kind = VK_OS_DRIVER_GETEVENTS;
		return;
	}
#endif

#if VK_USE_EPOLL
	if (vk_sys_caps_have_epoll(&fd_table_ptr->sys_caps)) {
		fd_table_ptr->poll_driver = VK_POLL_DRIVER_OS;
		fd_table_ptr->os_driver_kind = VK_OS_DRIVER_EPOLL;
		return;
	}
#endif

#if VK_USE_KQUEUE
	if (vk_sys_caps_have_kqueue(&fd_table_ptr->sys_caps)) {
		fd_table_ptr->poll_driver = VK_POLL_DRIVER_OS;
		fd_table_ptr->os_driver_kind = VK_OS_DRIVER_KQUEUE;
		return;
	}
#endif

#endif /* VK_USE_* */

    vk_fd_table_switch_to_poll(fd_table_ptr);
}

static void
vk_fd_table_switch_to_poll(struct vk_fd_table* fd_table_ptr)
{
    if (!fd_table_ptr) {
        return;
    }
    fd_table_ptr->poll_driver_requested = VK_POLL_DRIVER_POLL;
    fd_table_ptr->poll_driver = VK_POLL_DRIVER_POLL;
#if VK_USE_KQUEUE || VK_USE_EPOLL || VK_USE_GETEVENTS || VK_USE_IO_URING
    fd_table_ptr->os_driver_kind = VK_OS_DRIVER_NONE;
#endif
#if VK_USE_IO_URING
	vk_fd_table_uring_teardown(fd_table_ptr);
#endif
}

static void
vk_fd_table_os_deregister_fd(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
#if VK_USE_EPOLL || VK_USE_KQUEUE
    if (!fd_table_ptr || !fd_ptr) {
        return;
    }
#endif

#if VK_USE_EPOLL
    if (fd_table_ptr->os_driver_kind == VK_OS_DRIVER_EPOLL && fd_ptr->added && fd_table_ptr->epoll_initialized) {
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        if (epoll_ctl(fd_table_ptr->epoll_fd, EPOLL_CTL_DEL, fd_ptr->fd, &ev) == -1) {
            if (errno != EBADF && errno != ENOENT) {
                PERROR("epoll_ctl[DEL]");
            }
        }
        fd_ptr->added = 0;
    }
#endif

#if VK_USE_KQUEUE
    if (fd_table_ptr->os_driver_kind == VK_OS_DRIVER_KQUEUE && fd_ptr->added) {
        struct kevent ev[2];
        EV_SET(&ev[0], fd_ptr->fd, EVFILT_READ, EV_DELETE, 0, 0, fd_ptr);
        EV_SET(&ev[1], fd_ptr->fd, EVFILT_WRITE, EV_DELETE, 0, 0, fd_ptr);
        (void)kevent(fd_table_ptr->kq_fd, ev, 2, NULL, 0, NULL);
        fd_ptr->added = 0;
    }
#endif

    (void)fd_table_ptr;
    (void)fd_ptr;
}

size_t vk_fd_table_get_size(struct vk_fd_table* fd_table_ptr) { return fd_table_ptr->size; }
void vk_fd_table_set_size(struct vk_fd_table* fd_table_ptr, size_t size) { fd_table_ptr->size = size; }

struct vk_fd* vk_fd_table_get(struct vk_fd_table* fd_table_ptr, size_t i)
{
	if (i >= fd_table_ptr->size) {
		return NULL;
	}
	return &fd_table_ptr->fds[i];
}

struct vk_fd* vk_fd_table_first_dirty(struct vk_fd_table* fd_table_ptr)
{
	return SLIST_FIRST(&fd_table_ptr->dirty_fds);
}

struct vk_fd* vk_fd_table_first_fresh(struct vk_fd_table* fd_table_ptr)
{
	return SLIST_FIRST(&fd_table_ptr->fresh_fds);
}

void vk_fd_table_enqueue_dirty(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
	vk_fd_dbg("enqueuing to dirty");
	if (!fd_ptr->dirty_qed) {
		fd_ptr->dirty_qed = 1;
		SLIST_INSERT_HEAD(&fd_table_ptr->dirty_fds, fd_ptr, dirty_list_elem);
		vk_fd_dbg("enqueued to dirty");
	}
}

void vk_fd_table_enqueue_fresh(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
	vk_fd_dbg("enqueuing to fresh");
	if (!fd_ptr->fresh_qed) {
		fd_ptr->fresh_qed = 1;
		SLIST_INSERT_HEAD(&fd_table_ptr->fresh_fds, fd_ptr, fresh_list_elem);
		vk_fd_dbg("enqueued to fresh");
	}
}

void vk_fd_table_drop_dirty(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
	vk_fd_dbg("dropping from dirty queue");
	if (fd_ptr->dirty_qed) {
		fd_ptr->dirty_qed = 0;
		SLIST_REMOVE(&fd_table_ptr->dirty_fds, fd_ptr, vk_fd, dirty_list_elem);
		vk_fd_dbg("dropped from dirty queue");
	}
}

void vk_fd_table_drop_fresh(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
	vk_fd_dbg("dropping from fresh queue");
	if (fd_ptr->fresh_qed) {
		fd_ptr->fresh_qed = 0;
		SLIST_REMOVE(&fd_table_ptr->fresh_fds, fd_ptr, vk_fd, fresh_list_elem);
		vk_fd_dbg("dropped from fresh queue");
	}
}

struct vk_fd* vk_fd_table_dequeue_dirty(struct vk_fd_table* fd_table_ptr)
{
	struct vk_fd* fd_ptr;

	if (SLIST_EMPTY(&fd_table_ptr->dirty_fds)) {
		return NULL;
	}

	fd_ptr = SLIST_FIRST(&fd_table_ptr->dirty_fds);
	SLIST_REMOVE_HEAD(&fd_table_ptr->dirty_fds, dirty_list_elem);
	fd_ptr->dirty_qed = 0;
	vk_fd_dbg("dequeued to dirty");

	return fd_ptr;
}

struct vk_fd* vk_fd_table_dequeue_fresh(struct vk_fd_table* fd_table_ptr)
{
	struct vk_fd* fd_ptr;

	if (SLIST_EMPTY(&fd_table_ptr->fresh_fds)) {
		return NULL;
	}

	fd_ptr = SLIST_FIRST(&fd_table_ptr->fresh_fds);
	SLIST_REMOVE_HEAD(&fd_table_ptr->fresh_fds, fresh_list_elem);
	fd_ptr->fresh_qed = 0;
	vk_fd_dbg("dequeued to unfresh");

	return fd_ptr;
}

void vk_fd_table_dump(struct vk_fd_table* fd_table_ptr)
{
	size_t i;

        for (i = 0; i < fd_table_ptr->size; i++) {
                struct vk_fd* fd_ptr;

                fd_ptr = vk_fd_table_get(fd_table_ptr, i);
                if (fd_ptr != NULL && vk_fd_get_allocated(fd_ptr)) {
                        vk_fd_log("allocated");
                }
        }
}

/*
 * Poll Registration and Dispatch Glue
 */

/*
 * Enqueue blocked socket to the poller.
 * If needed, allocate a new FD for the socket.
 */
void vk_fd_table_prepoll_blocked_socket(struct vk_fd_table* fd_table_ptr, struct vk_socket* socket_ptr,
					struct vk_proc* proc_ptr)
{
	struct vk_io_future* rx_ioft_ptr;
	struct vk_io_future* tx_ioft_ptr;
	struct pollfd rx_event;
	struct pollfd tx_event;
	int rx_fd;
	int tx_fd;

	vk_socket_dbgf("prepoll for pid %zu.\n", vk_proc_get_id(proc_ptr));

	rx_ioft_ptr = vk_block_get_ioft_rx_pre(vk_socket_get_block(socket_ptr));
	rx_event = vk_io_future_get_event(rx_ioft_ptr);
	rx_fd = rx_event.fd;

	tx_ioft_ptr = vk_block_get_ioft_tx_pre(vk_socket_get_block(socket_ptr));
	tx_event = vk_io_future_get_event(tx_ioft_ptr);
	tx_fd = tx_event.fd;

	if (rx_fd != -1) {
		vk_fd_table_prepoll_blocked_fd(fd_table_ptr, socket_ptr, rx_ioft_ptr, proc_ptr);
	}
	if (tx_fd != -1 && tx_fd != rx_fd) {
		vk_fd_table_prepoll_blocked_fd(fd_table_ptr, socket_ptr, tx_ioft_ptr, proc_ptr);
	}
}

/*
 * Register a socket's FD. Called for each FD of `vk_fd_table_prepoll_blocked_socket()`.
 */
void vk_fd_table_prepoll_blocked_fd(struct vk_fd_table* fd_table_ptr, struct vk_socket* socket_ptr,
				    struct vk_io_future* ioft_ptr, struct vk_proc* proc_ptr)
{
	/* ioft_ptr is the socket's IO future */
	struct pollfd event;		  /* socket's IO future's event */
	struct vk_fd* fd_ptr;		  /* FD slot in table */
	struct vk_io_future* fd_ioft_ptr; /* FD slot's IO future */
	struct pollfd fd_event;		  /* FD slot's IO future's event */
	int fd;
	int rc;

	event = vk_io_future_get_event(ioft_ptr);
	vk_socket_dbgf("prepoll for pid %zu, FD %i, events %i\n", vk_proc_get_id(proc_ptr), event.fd, event.events);

	fd = event.fd;

	fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
	if (fd_ptr == NULL) {
		vk_socket_logf("prepoll for pid %zu, FD %i, no FD slot available.\n", vk_proc_get_id(proc_ptr), fd);
		return;
	}
	if (!vk_fd_get_allocated(fd_ptr)) {
		vk_socket_dbgf("allocating FD %i for pid %zu\n", fd, vk_proc_get_id(proc_ptr));
		rc = vk_proc_allocate_fd(proc_ptr, fd_ptr, fd);
		if (rc == -1) {
			vk_fd_perror("vk_proc_allocate_fd");
		}
	}

	fd_ioft_ptr = vk_fd_get_ioft_pre(fd_ptr);
	vk_io_future_init(fd_ioft_ptr, socket_ptr);
	fd_event = vk_io_future_get_event(fd_ioft_ptr);
	fd_event.fd = event.fd;
	fd_event.events = event.events;
	vk_io_future_set_event(fd_ioft_ptr, fd_event);
	vk_socket_dbgf("events = %i, rx_closed = %i, tx_closed = %i\n", fd_event.events, fd_ioft_ptr->rx_closed,
		       fd_ioft_ptr->tx_closed);
	if (fd_event.events != 0 || (fd_ioft_ptr->rx_closed && fd_ioft_ptr->tx_closed)) {
		vk_fd_dbg("polling");
		vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
	} else {
		vk_socket_dbgf("prepoll for pid %zu, FD %i, no events to poll for.\n", vk_proc_get_id(proc_ptr), fd);
	}

	vk_socket_dbgf("prepoll for pid %zu, FD %i, events %i\n", vk_proc_get_id(proc_ptr), fd_event.fd,
		       fd_event.events);
}

/* close, deallocate, and deregister all remaining FDs of zombie process */
void vk_fd_table_prepoll_zombie(struct vk_fd_table* fd_table_ptr, struct vk_proc* proc_ptr)
{
	struct vk_fd* cursor_fd_ptr;
	struct vk_fd* fd_ptr;

	vk_proc_dbg("close, deallocate, and deregister all remaining FDs of zombie");

	cursor_fd_ptr = vk_proc_first_fd(proc_ptr);
	while (cursor_fd_ptr) {
		fd_ptr = cursor_fd_ptr;
		/* walk the link to the next FD before the current FD is deallocated */
		cursor_fd_ptr = vk_fd_next_allocated_fd(cursor_fd_ptr);

		/* close, deallocate, and deregister FD associated with zombie */
		vk_io_future_set_rx_closed(vk_fd_get_ioft_pre(fd_ptr), 1);
		vk_io_future_set_tx_closed(vk_fd_get_ioft_pre(fd_ptr), 1);
		vk_fd_set_zombie(fd_ptr, 1);
		vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
		vk_proc_deallocate_fd(proc_ptr, fd_ptr);
	}
}

/* Event Driver Utilities */

/* interpret ioft_post.event from poller */
void vk_fd_table_process_fd(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
	/* mark the polled state */
	if (fd_ptr->ioft_post.event.revents & POLLIN) {
		fd_ptr->ioft_post.readable = 1;
	}
	if (fd_ptr->ioft_post.event.revents & POLLOUT) {
		fd_ptr->ioft_post.writable = 1;
	}
	if (fd_ptr->ioft_post.event.revents & POLLHUP) {
		fd_ptr->ioft_post.rx_closed = 1;
	}
#ifdef POLLRDHUP
	if (fd_ptr->ioft_post.event.revents & POLLRDHUP) {
		fd_ptr->ioft_post.tx_closed = 1;
	}
#endif
	if (fd_ptr->ioft_post.event.revents & POLLERR) {
		fd_ptr->ioft_post.rx_closed = 1;
	}

	/* return the polled state */
	fd_ptr->ioft_ret = fd_ptr->ioft_post;
	if (fd_ptr->ioft_ret.event.revents != 0) {
		/* only wake if events are reported */
		vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
		DBG("vk_fd_table_process_fd: fd=%d revents=0x%x\n", fd_ptr->fd, fd_ptr->ioft_ret.event.revents);
	}
	if ((fd_ptr->ioft_pre.event.events & fd_ptr->ioft_ret.event.revents) == fd_ptr->ioft_pre.event.events) {
		/* if any events are returned */

		/* drop returned events */
		fd_ptr->ioft_pre.event.events =
		    (short)((int)fd_ptr->ioft_pre.event.events ^
			    ((int)fd_ptr->ioft_ret.event.revents & (int)fd_ptr->ioft_ret.event.events));

		/* only drop FD if all of the events are handled */
		if (fd_ptr->ioft_pre.event.events == 0) {
			vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
		}
	}
}

/* handle FD closures */
void vk_fd_table_clean_fd(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
	if (fd_ptr->zombie) {
		vk_fd_dbg("dropping dirty zombie");
		vk_fd_table_os_deregister_fd(fd_table_ptr, fd_ptr);
		vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
		vk_fd_table_drop_fresh(fd_table_ptr, fd_ptr);
	}

	if (!fd_ptr->closed && (fd_ptr->zombie || (fd_ptr->ioft_pre.rx_closed && fd_ptr->ioft_pre.tx_closed))) {
		vk_fd_table_os_deregister_fd(fd_table_ptr, fd_ptr);
#if VK_USE_IO_URING
		if (fd_table_ptr && fd_table_ptr->os_driver_kind == VK_OS_DRIVER_IO_URING &&
		    fd_table_ptr->uring_initialized) {
			struct vk_fd_uring_state* ur = &fd_ptr->uring;
			if (ur->poll_armed) {
				ur->poll_armed = 0;
				ur->poll_meta.fd = NULL;
				ur->poll_meta.op = NULL;
			}
			if (ur->in_flight_rw) {
				struct vk_io_op* inflight = (struct vk_io_op*)ur->pending_rw;
				struct vk_io_queue* queue_ptr = vk_fd_get_io_queue(fd_ptr);
				ur->in_flight_rw = 0;
				ur->pending_rw = NULL;
				ur->rw_meta.fd = NULL;
				ur->rw_meta.op = NULL;
				if (inflight) {
					inflight->err = EBADF;
					inflight->res = -1;
					vk_io_op_set_state(inflight, VK_IO_OP_ERROR);
					vk_thread_io_complete_op(&fd_ptr->socket, queue_ptr, inflight);
				}
			}
		}
#endif
		/* if close is requested by not yet performed */
		/* close no longer deferred to poller
		vk_fd_dbgf("closing FD %i\n", fd_ptr->fd);
		rc = close(fd_ptr->fd);
		if (rc == -1 && errno == EINTR) {
		    rc = close(fd_ptr->fd);
		}
		if (rc == -1) {
		    vk_fd_perror("close");
		}
		*/
		fd_ptr->closed = 1;
		fd_ptr->ioft_post.rx_closed = 1;
		fd_ptr->ioft_post.tx_closed = 1;
		fd_ptr->ioft_ret.rx_closed = 1;
		fd_ptr->ioft_ret.tx_closed = 1;
		fd_ptr->ioft_pre.event.events = 0;
		/* closed -- nothing to poll for */
		vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
		if (!fd_ptr->zombie) {
			/* if not dead, keep it running until it dies completely */
			vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
		}
	}
}

/*
 * Event Drivers
 */

#if VK_USE_KQUEUE
/*
 * kqueue driver
 */
int vk_fd_table_kqueue_kevent(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr, int block)
{
	int rc;
	struct timespec timeout;
	int poll_error;
	int i;
	struct vk_fd* fd_ptr;
	struct kevent* ev_ptr;
	int fd;

	/* poll for events */

	timeout.tv_sec = block ? 1 : 0;
	timeout.tv_nsec = 0;

    do {
        vk_kern_receive_signal(kern_ptr);
        DBG("kevent driver=kqueue(kq=%i changes=%i max=%i block=%i)",
            fd_table_ptr->kq_fd, fd_table_ptr->kq_nchanges, VK_EV_MAX, block);
        rc = kevent(fd_table_ptr->kq_fd, fd_table_ptr->kq_changelist, fd_table_ptr->kq_nchanges,
                    fd_table_ptr->kq_eventlist, VK_EV_MAX, &timeout);
        poll_error = errno;
        DBG(" = %i\n", rc);
        vk_kern_receive_signal(kern_ptr);
        /*
         * loop if:
         *   - block requested, and no events yet, or
         *   - error is EINTR or EAGAIN
         */
    } while ((block && rc == 0) || (rc == -1 && (poll_error == EINTR || poll_error == EAGAIN)));
	if (rc == -1) {
		errno = poll_error;
		PERROR("kevent");
		return -1;
	}
	fd_table_ptr->kq_nchanges = 0;
	fd_table_ptr->kq_nevents = rc;

	/* dispatch events */

	for (i = 0; i < fd_table_ptr->kq_nevents; i++) {
		ev_ptr = &fd_table_ptr->kq_eventlist[i];
		fd = (int)(intptr_t)ev_ptr->udata;
		fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
		if (fd_ptr == NULL) {
			return -1;
		}
		if (ev_ptr->filter & EVFILT_READ) {
			fd_ptr->ioft_post.event.events &= ~POLLIN; /* reflect EV_ONESHOT */
			fd_ptr->ioft_post.event.revents |= POLLIN; /* return event */
			fd_ptr->ioft_post.readable =
			    ev_ptr->data; /* bytes available to be read on the socket receive queue */
		}
		if (ev_ptr->filter & EVFILT_WRITE) {
			fd_ptr->ioft_post.event.events &= ~POLLOUT; /* reflect EV_ONESHOT */
			fd_ptr->ioft_post.event.revents |= POLLOUT; /* return event */
			fd_ptr->ioft_post.writable =
			    ev_ptr->data; /* bytes available to be written to the socket send queue */
		}
		fd_ptr->ioft_ret = fd_ptr->ioft_post;
		vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
		vk_fd_dbg("dispatching event");
	}

	return 0;
}
int vk_fd_table_kqueue_set(struct vk_fd_table* fd_table_ptr, struct vk_fd* fd_ptr)
{
	struct kevent* ev_ptr;
	int need_read;
	int register_read;
	int registered_read;
	int need_write;
	int register_write;
	int registered_write;

	const char* need_read_str;
	const char* need_write_str;
	const char* sep;

	if (vk_fd_get_closed(fd_ptr)) {
		vk_fd_dbg("ignoring closed FD");
		return 0;
	}

	need_read_str = (fd_ptr->ioft_pre.event.events & POLLIN) ? "POLLIN" : "";
	need_write_str = (fd_ptr->ioft_pre.event.events & POLLOUT) ? "POLLOUT" : "";
	sep = (need_read_str[0] && need_write_str[0]) ? "+" : "";
	DBG("kqueue register driver=kqueue fd=%d needs=%s%s%s\n",
	    vk_fd_get_fd(fd_ptr), need_read_str, sep, need_write_str);

	need_read = fd_ptr->ioft_pre.event.events & POLLIN;
	need_write = fd_ptr->ioft_pre.event.events & POLLOUT;
	registered_read = fd_ptr->ioft_post.event.events & POLLIN;
	registered_write = fd_ptr->ioft_post.event.events & POLLOUT;
	register_read = need_read && (!registered_read);
	register_write = need_write && (!registered_write);

	if (register_read) {
		if (fd_table_ptr->kq_nchanges == VK_EV_MAX) {
			errno = ENOBUFS;
			return -1;
		}
		ev_ptr = &fd_table_ptr->kq_changelist[fd_table_ptr->kq_nchanges++];
		ev_ptr->ident = fd_ptr->fd;
		ev_ptr->filter = EVFILT_READ;
		if (fd_table_ptr->poll_method == VK_POLL_METHOD_EDGE_TRIGGERED) {
			/* The kqueue analog to edge-triggering is keeping registration while disabling on return */
			if (fd_ptr->added) {
				ev_ptr->flags = EV_ENABLE | EV_DISPATCH;
			} else {
				ev_ptr->flags = EV_ADD | EV_ENABLE | EV_DISPATCH;
				fd_ptr->added = 1;
			}
		} else {
			ev_ptr->flags = EV_ADD | EV_ONESHOT;
		}
		ev_ptr->udata = (void*)(intptr_t)fd_ptr->fd;
		DBG("kqueue stage driver=kqueue fd=%d filter=READ flags=0x%x\n",
		    fd_ptr->fd, (unsigned)ev_ptr->flags);
	}

	if (register_write) {
		if (fd_table_ptr->kq_nchanges == VK_EV_MAX) {
			if (register_read) {
				/* roll back */
				fd_table_ptr->kq_nchanges--;
			}
			errno = ENOBUFS;
			return -1;
		}
		ev_ptr = &fd_table_ptr->kq_changelist[fd_table_ptr->kq_nchanges++];
		ev_ptr->ident = fd_ptr->fd;
		ev_ptr->filter = EVFILT_WRITE;
		if (fd_table_ptr->poll_method == VK_POLL_METHOD_EDGE_TRIGGERED) {
			/* The kqueue analog to edge-triggering is keeping registration while disabling on return */
			if (fd_ptr->added) {
				ev_ptr->flags = EV_ENABLE | EV_DISPATCH;
			} else {
				ev_ptr->flags = EV_ADD | EV_ENABLE | EV_DISPATCH;
				fd_ptr->added = 1;
			}
		} else {
			ev_ptr->flags = EV_ADD | EV_ONESHOT;
		}
		ev_ptr->udata = (void*)(intptr_t)fd_ptr->fd;
		DBG("kqueue stage driver=kqueue fd=%d filter=WRITE flags=0x%x\n",
		    fd_ptr->fd, (unsigned)ev_ptr->flags);
	}

	fd_ptr->ioft_post = fd_ptr->ioft_pre;
	vk_fd_dbg("registered event");

	return 0;
}
int vk_fd_table_kqueue_full(struct vk_fd_table* fd_table_ptr) { return fd_table_ptr->kq_nchanges == VK_EV_MAX; }
int vk_fd_table_kqueue(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr)
{
	int rc;
	struct vk_fd* fd_ptr;

	if (!fd_table_ptr->kq_initialized) {
		rc = kqueue();
		if (rc == -1) {
			return -1;
		}
		fd_table_ptr->kq_fd = rc;
		fd_table_ptr->kq_initialized = 1;
	}

	while ((fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr))) {
		rc = vk_fd_table_kqueue_set(fd_table_ptr, fd_ptr);
		if (rc == -1) {
			if (errno == ENOBUFS) {
				/* flush registrations */
				rc = vk_fd_table_kqueue_kevent(fd_table_ptr, kern_ptr, 0);
				if (rc == -1) {
					return -1;
				}
				/* and retry */
				rc = vk_fd_table_kqueue_set(fd_table_ptr, fd_ptr);
				if (rc == -1) {
					return -1;
				}
			}
		}
	}

	rc = vk_fd_table_kqueue_kevent(fd_table_ptr, kern_ptr, 1);
	if (rc == -1) {
		return -1;
	}

	return 0;
}
#endif

#if VK_USE_EPOLL
/*
 * epoll driver
 */
int vk_fd_table_epoll(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr)
{
	int rc;
	struct vk_fd* fd_ptr;
	struct epoll_event ev;
	int fd;
	int ep_op;
	int poll_error;
	int i;

	if (!fd_table_ptr->epoll_initialized) {
		rc = epoll_create1(EPOLL_CLOEXEC);
		if (rc == -1) {
			if (errno == EPERM || errno == ENOSYS) {
				ERR("epoll_create1 unsupported (errno=%d); using poll driver\n", errno);
				fd_table_ptr->sys_caps.have_epoll = 0;
				fd_table_ptr->sys_caps_initialized = 1;
				vk_fd_table_switch_to_poll(fd_table_ptr);
				return 0;
			}
			return -1;
		}
		fd_table_ptr->epoll_fd = rc;
		fd_table_ptr->epoll_initialized = 1;
	}

	/* register/re-register dirty FDs */
	if (fd_table_ptr->poll_method == VK_POLL_METHOD_EDGE_TRIGGERED) {
		while ((fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr))) {
			if (fd_ptr->closed) {
				fd_ptr->ioft_post.event.events = 0;
				vk_fd_dbg("already closed");
			} else if (!fd_ptr->added) {
				ev.data.fd = fd_ptr->fd;
				ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
				ep_op = EPOLL_CTL_ADD;
				DBG("epoll_ctl(%i, %i, %i, [%i, %u])", fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd,
				    ev.data.fd, ev.events);
				rc = epoll_ctl(fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd, &ev);
				DBG(" = %i\n", rc);
				if (rc == -1) {
					if (errno == EPERM) {
						vk_fd_logf("epoll_ctl unsupported (errno=%d); switching to poll driver\n", errno);
						fd_table_ptr->sys_caps.have_epoll = 0;
						fd_table_ptr->sys_caps_initialized = 1;
						vk_fd_table_switch_to_poll(fd_table_ptr);
						vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
						return 0;
					}
					if (errno == EBADF) {
						vk_fd_logf("epoll_ctl saw closed FD (errno=%d); dropping fd=%d\n", errno, fd_ptr->fd);
						vk_fd_set_closed(fd_ptr, 1);
						fd_ptr->added = 0;
						vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
						continue;
					}
					PERROR("epoll_ctl");
					return -1;
				}
				fd_ptr->ioft_post = fd_ptr->ioft_pre;
				fd_ptr->added = 1;
			} else {
				vk_fd_dbg("already added edge-triggers");
			}
		}
	} else {
		while ((fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr))) {
			if (fd_ptr->closed) {
				fd_ptr->ioft_post.event.events = 0;
				vk_fd_dbg("already closed");
			} else {
				ev.data.fd = fd_ptr->fd;
				ev.events = 0;
				if (fd_ptr->ioft_pre.event.events & POLLOUT) {
					ev.events = EPOLLOUT | EPOLLONESHOT;
				}
				if (fd_ptr->ioft_pre.event.events & POLLIN) {
					ev.events = EPOLLIN | EPOLLONESHOT;
				}
				if (fd_ptr->added) {
					ep_op = EPOLL_CTL_MOD;
				} else {
					fd_ptr->added = 1;
					ep_op = EPOLL_CTL_ADD;
				}
				DBG("epoll_ctl(%i, %i, %i, [%i, %u])", fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd,
				    ev.data.fd, ev.events);
				rc = epoll_ctl(fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd, &ev);
				DBG(" = %i\n", rc);
				if (rc == -1) {
					if (errno == EPERM) {
						vk_fd_logf("epoll_ctl unsupported (errno=%d); switching to poll driver\n", errno);
						fd_table_ptr->sys_caps.have_epoll = 0;
						fd_table_ptr->sys_caps_initialized = 1;
						vk_fd_table_switch_to_poll(fd_table_ptr);
						vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
						return 0;
					}
					if (errno == EBADF) {
						vk_fd_logf("epoll_ctl saw closed FD (errno=%d); dropping fd=%d\n", errno, fd_ptr->fd);
						vk_fd_set_closed(fd_ptr, 1);
						fd_ptr->added = 0;
						vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
						continue;
					}
					PERROR("epoll_ctl");
					return -1;
				}
				fd_ptr->ioft_post = fd_ptr->ioft_pre;
			}
		}
	}

	if (!vk_kern_pending(kern_ptr)) {
		DBG("Cleanup stage has left nothing pending. Exit.\n");
		return 0;
	}

	/* get poll events */
    do {
        vk_kern_receive_signal(kern_ptr);
        DBG("epoll_wait(%i, ..., %i, 1000)", fd_table_ptr->epoll_fd, fd_table_ptr->epoll_event_count);
        rc = epoll_wait(fd_table_ptr->epoll_fd, fd_table_ptr->epoll_events, VK_EV_MAX, 1000);
        poll_error = errno;
        DBG(" = %i\n", rc);
        vk_kern_receive_signal(kern_ptr);
    } while (rc == 0 || (rc == -1 && (poll_error == EINTR || poll_error == EAGAIN)));
	if (rc == -1) {
		if (poll_error == EPERM) {
			vk_fd_logf("epoll_wait unsupported (errno=%d); switching to poll driver\n", poll_error);
			fd_table_ptr->sys_caps.have_epoll = 0;
			fd_table_ptr->sys_caps_initialized = 1;
			vk_fd_table_switch_to_poll(fd_table_ptr);
			return 0;
		}
		errno = poll_error;
		PERROR("epoll_wait");
		return -1;
	}
	fd_table_ptr->epoll_event_count = rc;

	/* process poll events */
	for (i = 0; i < fd_table_ptr->epoll_event_count; i++) {
		ev = fd_table_ptr->epoll_events[i];
		fd = ev.data.fd;
		fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
		vk_fd_dbgf("epoll event: ev.data.fd = %i; ev.events = %u\n", ev.data.fd, ev.events);
		DBG("epoll_wait event: fd=%d events=0x%x\n", fd, ev.events);
		if (fd_ptr == NULL) {
			return -1;
		}
		fd_ptr->ioft_post.event.revents = 0;
		if (ev.events & EPOLLIN) {
			fd_ptr->ioft_post.event.revents |= POLLIN;
		}
		if (ev.events & EPOLLOUT) {
			fd_ptr->ioft_post.event.revents |= POLLOUT;
		}
		if (ev.events & EPOLLHUP) {
			fd_ptr->ioft_post.event.revents |= POLLHUP;
		}
		if (ev.events & EPOLLRDHUP) {
			fd_ptr->ioft_post.event.revents |= POLLRDHUP;
		}
		if (ev.events & EPOLLERR) {
			fd_ptr->ioft_post.event.revents |= POLLERR;
		}

		vk_fd_table_process_fd(fd_table_ptr, fd_ptr);

		vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);
	}

	return 0;
}
#endif

#if VK_USE_GETEVENTS
/*
 * io_getevents driver (AIO)
 */
int vk_fd_table_aio_setup(struct vk_fd_table* fd_table_ptr)
{
	long rc;

	fd_table_ptr->aio_ctx = 0;
	DBG("io_setup(VK_FD_MAX, %p)", &fd_table_ptr->aio_ctx);
	rc = syscall(SYS_io_setup, VK_FD_MAX, &fd_table_ptr->aio_ctx);
	DBG(" = %li\n", rc);
	if (rc == -1) {
		PERROR("io_setup");
		return -1;
	}

	return 0;
}
int vk_fd_table_aio(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr)
{
	const size_t max_entries = (size_t)VK_FD_MAX * 2;
	struct iocb* submit_list[max_entries];
	struct vk_fd_aio_meta* meta_list[max_entries];
	struct io_event events[max_entries];
	struct vk_io_libaio_batch batch;
	struct vk_io_batch* batch_if;
	struct vk_fd* fd_ptr;
	long rc;
	size_t submitted = 0;
	int submit_err = 0;
	int disable_aio_driver = 0;

	vk_io_batch_libaio_init(&batch, submit_list, meta_list, events, max_entries);
	batch_if = &batch.base;

	if (fd_table_ptr->aio_initialized == 0) {
		rc = vk_fd_table_aio_setup(fd_table_ptr);
		if (rc == -1) {
			return -1;
		}
		fd_table_ptr->aio_initialized = 1;
	}

	while ((fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr))) {
		if (vk_io_batch_count(batch_if) >= max_entries) {
			vk_fd_logf("submit_count %zu >= %zu -- skipping fd %d\n",
			           vk_io_batch_count(batch_if), max_entries, fd_ptr->fd);
			vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
			break;
		}

		vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);

		if (fd_ptr->closed) {
			continue;
		}

		if (fd_ptr->ioft_pre.event.events != 0) {
			if (vk_io_batch_stage_poll(batch_if, fd_ptr) == -1) {
				vk_fd_logf("submit_count %zu >= %zu -- skipping fd %d\n",
				           vk_io_batch_count(batch_if), max_entries, fd_ptr->fd);
				vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
				break;
			}
		}

		struct vk_io_queue* queue_ptr = vk_fd_get_io_queue(fd_ptr);
		struct vk_io_op* op_ptr = queue_ptr ? vk_io_queue_first_phys(queue_ptr) : NULL;
		if (op_ptr && vk_io_op_get_state(op_ptr) == VK_IO_OP_STAGED &&
		    vk_fd_has_cap(fd_ptr, VK_FD_CAP_AIO_RW)) {
			enum VK_IO_OP_KIND kind = vk_io_op_get_kind(op_ptr);
			if (kind == VK_IO_READ || kind == VK_IO_WRITE) {
				if (vk_io_batch_stage_rw(batch_if, fd_ptr, op_ptr) == -1) {
					vk_fd_logf("submit_count %zu >= %zu -- skipping data submit fd %d\n",
					           vk_io_batch_count(batch_if), max_entries, fd_ptr->fd);
					vk_io_op_set_state(op_ptr, VK_IO_OP_PENDING);
					vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
					continue;
				}
			} else {
				vk_io_op_set_state(op_ptr, VK_IO_OP_PENDING);
			}
		} else if (op_ptr && vk_io_op_get_state(op_ptr) == VK_IO_OP_STAGED) {
			vk_io_op_set_state(op_ptr, VK_IO_OP_PENDING);
		}
	}

	if (!vk_kern_pending(kern_ptr)) {
		DBG("Cleanup stage has left nothing pending. Exit.\n");
		return 0;
	}

	if (vk_io_batch_count(batch_if) == 0) {
		return 0;
	}

	if (vk_io_batch_submit(batch_if, fd_table_ptr, kern_ptr, &submitted, &submit_err) == -1) {
		return -1;
	}

	if (submitted < vk_io_batch_count(batch_if) || submit_err != 0) {
		vk_io_batch_post_submit(batch_if, submitted, submit_err, fd_table_ptr, &disable_aio_driver);
	}

	if (disable_aio_driver || submit_err == EINVAL || submit_err == EOPNOTSUPP) {
		DBG("io_submit poll unsupported (errno=%d); switching to epoll/poll fallback\n", submit_err);
		if (fd_table_ptr->aio_ctx != 0) {
			long destroy_rc = syscall(SYS_io_destroy, fd_table_ptr->aio_ctx);
			if (destroy_rc == -1) {
				PERROR("io_destroy");
			}
			fd_table_ptr->aio_ctx = 0;
			fd_table_ptr->aio_initialized = 0;
		}
		fd_table_ptr->sys_caps.have_aio_poll = 0;
		fd_table_ptr->sys_caps_initialized = 1;
		if (submit_err == EINVAL || submit_err == EOPNOTSUPP) {
			vk_fd_table_switch_to_poll(fd_table_ptr);
			return 0;
		}
		if (vk_io_batch_had_poll_ops(batch_if)) {
			return 0;
		}
	}

	if (vk_io_batch_reap(batch_if, fd_table_ptr, kern_ptr, submitted) == -1) {
		return -1;
	}

	return 0;
}

#endif

#if !VK_USE_KQUEUE && !VK_USE_EPOLL && !VK_USE_GETEVENTS
#error "No poll driver defined"
#endif
/*
 * poll driver
 */
int vk_fd_table_poll(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr)
{
	int rc;
	struct vk_fd* fd_ptr;
	nfds_t i;
	int fd;
	int poll_error;

	/* register/re-register dirty FDs and handle FD closures */
	fd_table_ptr->poll_nfds = 0;
	fd_ptr = vk_fd_table_first_dirty(fd_table_ptr);
	while (fd_ptr) {
		/* Handle closures first, because closure ends polling. */
		vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);

		/* build the poll array */
		if (fd_table_ptr->poll_nfds < VK_FD_MAX) {
			if (!fd_ptr->closed && fd_ptr->ioft_pre.event.events != 0) {
				fd_ptr->ioft_post = fd_ptr->ioft_pre;
				DBG("poll register driver=poll fd=%d events=0x%x\n",
				    fd_ptr->fd, (unsigned)fd_ptr->ioft_post.event.events);
				fd_table_ptr->poll_fds[fd_table_ptr->poll_nfds++] = fd_ptr->ioft_post.event;
			}
		}

		fd_ptr = vk_fd_next_dirty_fd(fd_ptr);
	}

	if (!vk_kern_pending(kern_ptr)) {
		DBG("Cleanup stage has left nothing pending. Exit.\n");
		return 0;
	}

	/* get poll events */
    do {
        vk_kern_receive_signal(kern_ptr);
        DBG("poll driver=poll(nfds=%li timeout=1000)", (long int)fd_table_ptr->poll_nfds);
        rc = poll(fd_table_ptr->poll_fds, fd_table_ptr->poll_nfds, 1000);
        poll_error = errno;
        DBG(" = %i\n", rc);
        vk_kern_receive_signal(kern_ptr);
    } while (rc == 0 || (rc == -1 && (poll_error == EINTR || poll_error == EAGAIN)));
    if (rc == -1) {
        errno = poll_error;
        PERROR("poll");
        return -1;
    }
	DBG("poll driver=poll rc=%d\n", rc);

    /* process poll events */
    for (i = 0; i < fd_table_ptr->poll_nfds; i++) {
        fd = fd_table_ptr->poll_fds[i].fd;
        fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
        if (fd_ptr == NULL) {
            return -1;
        }
		DBG("poll: fd=%d revents=0x%x\n", fd, fd_ptr->ioft_post.event.revents);

        /* mark the polled state */
        fd_ptr->ioft_post.event = fd_table_ptr->poll_fds[i];

        vk_fd_table_process_fd(fd_table_ptr, fd_ptr);
	}

	return 0;
}

int vk_fd_table_wait(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr)
{
	vk_fd_table_select_driver(fd_table_ptr);
	if (fd_table_ptr->poll_driver == VK_POLL_DRIVER_POLL) {
		return vk_fd_table_poll(fd_table_ptr, kern_ptr);
	}

#if VK_USE_KQUEUE || VK_USE_EPOLL || VK_USE_GETEVENTS || VK_USE_IO_URING
	switch (fd_table_ptr->os_driver_kind) {
#if VK_USE_KQUEUE
	case VK_OS_DRIVER_KQUEUE:
		return vk_fd_table_kqueue(fd_table_ptr, kern_ptr);
#endif
#if VK_USE_EPOLL
	case VK_OS_DRIVER_EPOLL:
		return vk_fd_table_epoll(fd_table_ptr, kern_ptr);
#endif
#if VK_USE_GETEVENTS
	case VK_OS_DRIVER_GETEVENTS:
		return vk_fd_table_aio(fd_table_ptr, kern_ptr);
#endif
#if VK_USE_IO_URING
	case VK_OS_DRIVER_IO_URING:
		return vk_fd_table_uring(fd_table_ptr, kern_ptr);
#endif
	case VK_OS_DRIVER_NONE:
	default:
		break;
	}
#endif

	errno = EINVAL;
	return -1;
}

#if VK_USE_IO_URING
static int
vk_fd_table_uring_setup(struct vk_fd_table* fd_table_ptr)
{
	if (!fd_table_ptr) {
		return -1;
	}
	if (fd_table_ptr->uring_initialized) {
		return 0;
	}
	unsigned depth = VK_IO_URING_QUEUE_DEPTH;
	int rc = io_uring_queue_init(depth, &fd_table_ptr->uring_driver.ring, 0);
	if (rc < 0) {
		errno = -rc;
		PERROR("io_uring_queue_init");
		return -1;
	}
	fd_table_ptr->uring_driver.queue_depth = depth;
	fd_table_ptr->uring_driver.pending_sqes = 0;
	fd_table_ptr->uring_driver.pending_poll = 0;
	fd_table_ptr->uring_driver.pending_rw = 0;
	fd_table_ptr->uring_driver.wait_timeout.tv_sec = 1;
	fd_table_ptr->uring_driver.wait_timeout.tv_nsec = 0;
	fd_table_ptr->uring_initialized = 1;
	return 0;
}

static void
vk_fd_table_uring_teardown(struct vk_fd_table* fd_table_ptr)
{
	if (!fd_table_ptr) {
		return;
	}
	if (fd_table_ptr->uring_initialized) {
		io_uring_queue_exit(&fd_table_ptr->uring_driver.ring);
		fd_table_ptr->uring_initialized = 0;
	}
	memset(&fd_table_ptr->uring_driver, 0, sizeof(fd_table_ptr->uring_driver));
}

static int
vk_fd_table_uring(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr)
{
	if (!fd_table_ptr) {
		errno = EINVAL;
		return -1;
	}

	if (!fd_table_ptr->uring_initialized) {
		if (vk_fd_table_uring_setup(fd_table_ptr) == -1) {
			vk_fd_table_switch_to_poll(fd_table_ptr);
			return -1;
		}
	}

	struct vk_io_uring_driver* driver = &fd_table_ptr->uring_driver;
	size_t max_entries = driver->queue_depth ? driver->queue_depth : VK_IO_URING_QUEUE_DEPTH;
	if (max_entries == 0) {
		errno = EINVAL;
		return -1;
	}

	struct vk_fd_uring_meta* meta_list[max_entries];
	struct vk_io_uring_batch batch;
	size_t submitted = 0;
	int submit_err = 0;
	int disable_driver = 0;
	struct vk_fd* fd_ptr;

	vk_io_batch_uring_init(&batch, driver, meta_list, max_entries);

	while ((fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr))) {
		if (vk_io_batch_count(&batch.base) >= max_entries) {
			vk_fd_logf("submit_count %zu >= %zu -- skipping fd %d\n",
			           vk_io_batch_count(&batch.base), max_entries, fd_ptr->fd);
			vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
			break;
		}

		vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);

		if (fd_ptr->closed) {
			continue;
		}

		if (fd_ptr->ioft_pre.event.events != 0) {
			if (vk_io_batch_stage_poll(&batch.base, fd_ptr) == -1) {
				vk_fd_logf("submit_count %zu >= %zu -- skipping fd %d\n",
				           vk_io_batch_count(&batch.base), max_entries, fd_ptr->fd);
				vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
				break;
			}
		}

		struct vk_io_queue* queue_ptr = vk_fd_get_io_queue(fd_ptr);
		struct vk_io_op* op_ptr = queue_ptr ? vk_io_queue_first_phys(queue_ptr) : NULL;
		if (op_ptr && vk_io_op_get_state(op_ptr) == VK_IO_OP_STAGED &&
		    vk_fd_has_cap(fd_ptr, VK_FD_CAP_IO_URING)) {
			enum VK_IO_OP_KIND kind = vk_io_op_get_kind(op_ptr);
			if (kind == VK_IO_READ || kind == VK_IO_WRITE) {
				if (vk_io_batch_stage_rw(&batch.base, fd_ptr, op_ptr) == -1) {
					vk_fd_logf("submit_count %zu >= %zu -- skipping data submit fd %d\n",
					           vk_io_batch_count(&batch.base), max_entries, fd_ptr->fd);
					vk_io_op_set_state(op_ptr, VK_IO_OP_PENDING);
					vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
					continue;
				}
			} else {
				vk_io_op_set_state(op_ptr, VK_IO_OP_PENDING);
			}
		} else if (op_ptr && vk_io_op_get_state(op_ptr) == VK_IO_OP_STAGED) {
			vk_io_op_set_state(op_ptr, VK_IO_OP_PENDING);
		}
	}

	if (!vk_kern_pending(kern_ptr)) {
		DBG("Cleanup stage has left nothing pending. Exit.\n");
		vk_io_batch_reset(&batch.base);
		return 0;
	}

	size_t staged_total = vk_io_batch_count(&batch.base);
	if (staged_total == 0) {
		vk_io_batch_reap(&batch.base, fd_table_ptr, kern_ptr, 0);
		vk_io_batch_reset(&batch.base);
		return 0;
	}

	if (vk_io_batch_submit(&batch.base, fd_table_ptr, kern_ptr, &submitted, &submit_err) == -1) {
		vk_io_batch_reset(&batch.base);
		return -1;
	}
	DBG("io_uring submit(count=%zu submitted=%zu err=%d)\n",
	    staged_total, submitted, submit_err);

	if (submitted < staged_total || submit_err != 0) {
		vk_io_batch_post_submit(&batch.base, submitted, submit_err, fd_table_ptr, &disable_driver);
	}

	if (disable_driver || submit_err == EINVAL || submit_err == EOPNOTSUPP) {
		DBG("io_uring unsupported (errno=%d); switching to poll fallback\n", submit_err);
		vk_fd_table_uring_teardown(fd_table_ptr);
		fd_table_ptr->sys_caps.have_io_uring = 0;
		fd_table_ptr->sys_caps_initialized = 1;
		vk_fd_table_switch_to_poll(fd_table_ptr);
		vk_io_batch_reset(&batch.base);
		return 0;
	}

	if (submit_err == EAGAIN || submit_err == EBUSY || submit_err == EINTR) {
		vk_io_batch_reset(&batch.base);
		return 0;
	}

	if (submitted == 0) {
		vk_io_batch_reset(&batch.base);
		return 0;
	}

	if (vk_io_batch_reap(&batch.base, fd_table_ptr, kern_ptr, submitted) == -1) {
		vk_io_batch_reset(&batch.base);
		return -1;
	}

	vk_io_batch_reset(&batch.base);
	return 0;
}
#endif
