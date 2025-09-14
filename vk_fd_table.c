#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "vk_fd_table.h"
#include "vk_debug.h"
#include "vk_fd.h"
#include "vk_fd_table_s.h"
#include "vk_io_future.h"
#include "vk_kern.h"
#include "vk_proc.h"
#include "vk_socket.h"

#include <errno.h>
#include <stddef.h>

/*
 * Object Manipulation
 */
size_t vk_fd_table_object_size() { return sizeof (struct vk_fd_table); }
size_t vk_fd_table_entry_size() { return sizeof (struct vk_fd); }
size_t vk_fd_table_alloc_size(size_t size) { return sizeof(struct vk_fd_table) + (sizeof(struct vk_fd) * size); }

enum vk_poll_driver vk_fd_table_get_poll_driver(struct vk_fd_table* fd_table_ptr) { return fd_table_ptr->poll_driver; }
void vk_fd_table_set_poll_driver(struct vk_fd_table* fd_table_ptr, enum vk_poll_driver poll_driver)
{
	fd_table_ptr->poll_driver = poll_driver;
}

enum vk_poll_method vk_fd_table_get_poll_method(struct vk_fd_table* fd_table_ptr) { return fd_table_ptr->poll_method; }
void vk_fd_table_set_poll_method(struct vk_fd_table* fd_table_ptr, enum vk_poll_method poll_method)
{
	fd_table_ptr->poll_method = poll_method;
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
		vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
		vk_fd_table_drop_fresh(fd_table_ptr, fd_ptr);
	}

	if (!fd_ptr->closed && (fd_ptr->zombie || (fd_ptr->ioft_pre.rx_closed && fd_ptr->ioft_pre.tx_closed))) {
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

#if defined(VK_USE_KQUEUE)
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
        DBG("kevent(%i, ..., %i, ..., %i, %i)", fd_table_ptr->kq_fd, fd_table_ptr->kq_nchanges, VK_EV_MAX,
            block);
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

	/* batch the registration of the event */
	vk_fd_dbg("registering event");

	if (vk_fd_get_closed(fd_ptr)) {
		vk_fd_dbg("ignoring closed FD");
		return 0;
	}

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
#elif defined(VK_USE_EPOLL)
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
					PERROR("epoll_ctl");
					return -1;
				}
				fd_ptr->ioft_post = fd_ptr->ioft_pre;
			}
		}
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
#elif defined(VK_USE_GETEVENTS)
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
	long rc;
	nfds_t fd_cursor;
	struct timespec timeout;
	struct vk_fd* fd_ptr;
	nfds_t iocb_list_count;
	struct iocb* iocb_list[VK_FD_MAX];
	struct io_event events[VK_FD_MAX];
	int i;

	if (fd_table_ptr->aio_initialized == 0) {
		rc = vk_fd_table_aio_setup(fd_table_ptr);
		if (rc == -1) {
			return -1;
		}
		fd_table_ptr->aio_initialized = 1;
	}

	iocb_list_count = 0;
	while ((fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr))) {
		if (iocb_list_count >= VK_FD_MAX) {
			vk_fd_logf("iocb_list_count %i >= VK_FD_MAX %i -- skipping registrations\n",
				   (int)iocb_list_count, (int)VK_FD_MAX);
			break;
		}

		vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);

		if (fd_ptr->closed || fd_ptr->ioft_pre.event.events == 0) {
			continue;
		}

		fd_ptr->ioft_post = fd_ptr->ioft_pre;

		fd_ptr->iocb.aio_fildes = fd_ptr->fd;
		fd_ptr->iocb.aio_lio_opcode = IOCB_CMD_POLL;
		fd_ptr->iocb.aio_reqprio = 0;
		fd_ptr->iocb.aio_buf = fd_ptr->ioft_pre.event.events;

		iocb_list[iocb_list_count++] = &fd_ptr->iocb;
	}

	if (!vk_kern_pending(kern_ptr)) {
		DBG("Cleanup stage has left nothing pending. Exit.\n");
		return 0;
	}

	vk_kern_receive_signal(kern_ptr);
	DBG("io_submit(%p, %li, ...)", &fd_table_ptr->aio_ctx, (long int)iocb_list_count);
	rc = syscall(SYS_io_submit, fd_table_ptr->aio_ctx, iocb_list_count, iocb_list);
	DBG(" = %li\n", rc);
	vk_kern_receive_signal(kern_ptr);
	if (rc == -1) {
		if (errno == EAGAIN) {
			vk_klog("io_submit() returned EAGAIN -- waiting for next loop");
			rc = 0;
			errno = 0;
		} else {
			PERROR("io_submit");
			return -1;
		}
	}
	fd_cursor = rc;

	while (fd_cursor < iocb_list_count) {
		/* Not all got submitted. Attempt to drain ready events without blocking. */
		vk_kern_receive_signal(kern_ptr);
		DBG("io_getevents[B](%p, 1, VK_FD_MAX, ..., 1)", &fd_table_ptr->aio_ctx);
		timeout.tv_sec = 0;
		timeout.tv_nsec = 0;
		rc = syscall(SYS_io_getevents, fd_table_ptr->aio_ctx, 1, VK_FD_MAX, events, &timeout);
		DBG(" = %li\n", rc);
		vk_kern_receive_signal(kern_ptr);
		if (rc == -1) {
			if (errno == EINTR) {
				continue;
			}
			PERROR("io_getevents[B]");
			return -1;
		}

		if (rc > 0) {
			/* Something got drained. Attempt to submit more. */
			vk_kern_receive_signal(kern_ptr);
			DBG("io_submit[B](%p, %li, ...)", &fd_table_ptr->aio_ctx, (long int)iocb_list_count);
			rc = syscall(SYS_io_submit, fd_table_ptr->aio_ctx, iocb_list_count - fd_cursor,
				     iocb_list + fd_cursor);
			DBG(" = %li\n", rc);
			vk_kern_receive_signal(kern_ptr);
			if (rc == -1) {
				if (errno == EAGAIN) {
					rc = 0;
					errno = 0;
				} else if (errno == EINTR) {
					continue;
				} else {
					PERROR("io_submit[B]");
					return -1;
				}
			}
			fd_cursor += rc;
		} else {
			break;
		}
	}

	/* Drain ready events with blocking. */
	vk_kern_receive_signal(kern_ptr);
	DBG("io_getevents(%p, 1, VK_FD_MAX, ..., 1)", &fd_table_ptr->aio_ctx);
	timeout.tv_sec = 1;
	timeout.tv_nsec = 0;
	rc = syscall(SYS_io_getevents, fd_table_ptr->aio_ctx, 1, VK_FD_MAX, events, &timeout);
	DBG(" = %li\n", rc);
	vk_kern_receive_signal(kern_ptr);
	if (rc == -1) {
		if (errno == EINTR) {
			rc = 0;
			errno = 0;
		} else {
			PERROR("io_getevents");
			return -1;
		}
	}

	for (i = 0; i < rc; i++) {
		struct io_event event = events[i];
		fd_ptr = vk_fd_table_get(fd_table_ptr, ((struct iocb*)event.obj)->aio_fildes);
		if (fd_ptr == NULL) {
			return -1;
		}

		fd_ptr->ioft_post.event = fd_ptr->ioft_pre.event;
		fd_ptr->ioft_post.event.revents = (short)event.res;

		vk_fd_table_process_fd(fd_table_ptr, fd_ptr);

		vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);
	}

	return 0;
}
#else
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
        DBG("poll(..., %li, 1000)", (long int)fd_table_ptr->poll_nfds);
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

	/* process poll events */
	for (i = 0; i < fd_table_ptr->poll_nfds; i++) {
		fd = fd_table_ptr->poll_fds[i].fd;
		fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
		if (fd_ptr == NULL) {
			return -1;
		}

		/* mark the polled state */
		fd_ptr->ioft_post.event = fd_table_ptr->poll_fds[i];

		vk_fd_table_process_fd(fd_table_ptr, fd_ptr);
	}

	return 0;
}

int vk_fd_table_wait(struct vk_fd_table* fd_table_ptr, struct vk_kern* kern_ptr)
{
	if (fd_table_ptr->poll_driver == VK_POLL_DRIVER_POLL) {
		return vk_fd_table_poll(fd_table_ptr, kern_ptr);
	} else {
#if defined(VK_USE_KQUEUE)
		return vk_fd_table_kqueue(fd_table_ptr, kern_ptr);
#elif defined(VK_USE_EPOLL)
		return vk_fd_table_epoll(fd_table_ptr, kern_ptr);
#elif defined(VK_USE_GETEVENTS)
		return vk_fd_table_aio(fd_table_ptr, kern_ptr);
#else
		errno = EINVAL;
		return -1;
#endif
	}
}
