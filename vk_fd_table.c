#include "vk_debug.h"
#include "vk_fd_table.h"
#include "vk_fd_table_s.h"
#include "vk_io_future.h"
#include "vk_kern.h"

#include <stddef.h>
#include <errno.h>


size_t vk_fd_table_alloc_size(size_t size) {
	return sizeof (struct vk_fd_table) + (sizeof (struct vk_fd) * size);
}

enum vk_poll_driver vk_fd_table_get_poll_driver(struct vk_fd_table *fd_table_ptr) {
	return fd_table_ptr->poll_driver;
}
void vk_fd_table_set_poll_driver(struct vk_fd_table *fd_table_ptr, enum vk_poll_driver poll_driver) {
	fd_table_ptr->poll_driver = poll_driver;
}

enum vk_poll_method vk_fd_table_get_poll_method(struct vk_fd_table *fd_table_ptr) {
	return fd_table_ptr->poll_method;
}
void vk_fd_table_set_poll_method(struct vk_fd_table *fd_table_ptr, enum vk_poll_method poll_method) {
	fd_table_ptr->poll_method = poll_method;
}

size_t vk_fd_table_get_size(struct vk_fd_table *fd_table_ptr) {
	return fd_table_ptr->size;
}
void vk_fd_table_set_size(struct vk_fd_table *fd_table_ptr, size_t size) {
	fd_table_ptr->size = size;
}

struct vk_fd *vk_fd_table_get(struct vk_fd_table *fd_table_ptr, size_t i) {
	if (i >= fd_table_ptr->size) {
		return NULL;
	}
	return &fd_table_ptr->fds[i];
}

struct vk_fd *vk_fd_table_first_dirty(struct vk_fd_table *fd_table_ptr) {
    return SLIST_FIRST(&fd_table_ptr->dirty_fds);
}

struct vk_fd *vk_fd_table_first_fresh(struct vk_fd_table *fd_table_ptr) {
    return SLIST_FIRST(&fd_table_ptr->fresh_fds);
}

void vk_fd_table_enqueue_dirty(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
    vk_fd_dbg("enqueuing to dirty");
    if ( ! fd_ptr->dirty_qed) {
        fd_ptr->dirty_qed = 1;
        SLIST_INSERT_HEAD(&fd_table_ptr->dirty_fds, fd_ptr, dirty_list_elem);
        vk_fd_dbg("enqueued to dirty");
    }
}

void vk_fd_table_enqueue_fresh(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
    vk_fd_dbg("enqueuing to fresh");
    if ( ! fd_ptr->fresh_qed) {
        fd_ptr->fresh_qed = 1;
        SLIST_INSERT_HEAD(&fd_table_ptr->fresh_fds, fd_ptr, fresh_list_elem);
        vk_fd_dbg("enqueued to fresh");
    }
}

void vk_fd_table_drop_dirty(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
    vk_fd_dbg("dropping from dirty queue");
    if (fd_ptr->dirty_qed) {
        fd_ptr->dirty_qed = 0;
        SLIST_REMOVE(&fd_table_ptr->dirty_fds, fd_ptr, vk_fd, dirty_list_elem);
        vk_fd_dbg("dropped from dirty queue");
    }
}

void vk_fd_table_drop_fresh(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
    vk_fd_dbg("dropping from fresh queue");
    if (fd_ptr->fresh_qed) {
        fd_ptr->fresh_qed = 0;
        SLIST_REMOVE(&fd_table_ptr->fresh_fds, fd_ptr, vk_fd, fresh_list_elem);
        vk_fd_dbg("dropped from fresh queue");
    }
}

struct vk_fd *vk_fd_table_dequeue_dirty(struct vk_fd_table *fd_table_ptr) {
    struct vk_fd *fd_ptr;

    if (SLIST_EMPTY(&fd_table_ptr->dirty_fds)) {
        return NULL;
    }

    fd_ptr = SLIST_FIRST(&fd_table_ptr->dirty_fds);
    SLIST_REMOVE_HEAD(&fd_table_ptr->dirty_fds, dirty_list_elem);
    fd_ptr->dirty_qed = 0;
    vk_fd_dbg("dequeued to dirty");

    return fd_ptr;
}

struct vk_fd *vk_fd_table_dequeue_fresh(struct vk_fd_table *fd_table_ptr) {
    struct vk_fd *fd_ptr;

    if (SLIST_EMPTY(&fd_table_ptr->fresh_fds)) {
        return NULL;
    }

    fd_ptr = SLIST_FIRST(&fd_table_ptr->fresh_fds);
    SLIST_REMOVE_HEAD(&fd_table_ptr->fresh_fds, fresh_list_elem);
    fd_ptr->fresh_qed = 0;
    vk_fd_dbg("dequeued to unfresh");

    return fd_ptr;
}


void vk_fd_table_prepoll_blocked_socket(struct vk_fd_table *fd_table_ptr, struct vk_socket *socket_ptr, struct vk_proc *proc_ptr) {
	struct vk_fd *fd_ptr;
	struct vk_io_future *ioft_ptr;
	struct pollfd event;
	int fd;

	fd = vk_socket_get_blocked_fd(socket_ptr);
	if (fd == -1) {
		vk_socket_dbgf("prepoll for pid %zu, socket is not blocked on FD, so nothing to poll for it.\n", vk_proc_get_id(proc_ptr));
		return;
	}
    vk_socket_dbgf("prepoll for FD %i\n", fd);

	fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
	if (fd_ptr == NULL) {
		return;
	}
    if ( ! vk_fd_get_allocated(fd_ptr)) {
        vk_proc_allocate_fd(proc_ptr, fd_ptr, fd);
    }
    if (vk_fd_get_proc_id(fd_ptr) != vk_proc_get_id(proc_ptr)) {
        vk_socket_dbgf("prepoll for pid %zu, FD %i, already allocated by process %zu, so abandoning poll.\n", vk_proc_get_id(proc_ptr), fd, vk_fd_get_proc_id(fd_ptr));
        return;
    }
	ioft_ptr = vk_fd_get_ioft_pre(fd_ptr);
	vk_io_future_init(ioft_ptr, socket_ptr);
	event = vk_io_future_get_event(ioft_ptr);
	vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);

	vk_socket_dbgf("prepoll for pid %zu, FD %i, events %i\n", vk_proc_get_id(proc_ptr), event.fd, event.events);

	return;
}

void vk_fd_table_prepoll_fd(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
    vk_fd_dbgf("prepoll for FD %i\n", fd_ptr->fd);
    if (fd_ptr->closed) {
        vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
    }
}

void vk_fd_table_prepoll_zombie(struct vk_fd_table *fd_table_ptr, struct vk_proc *proc_ptr) {
	struct vk_fd *cursor_fd_ptr;
	struct vk_fd *fd_ptr;

	vk_proc_dbg("deallocate all remaining FDs of zombie");

	cursor_fd_ptr = vk_proc_first_fd(proc_ptr);
	while (cursor_fd_ptr) {
		fd_ptr = cursor_fd_ptr;
		cursor_fd_ptr = vk_fd_next_allocated_fd(cursor_fd_ptr);

		vk_fd_set_closed(fd_ptr, 1);
		vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
		vk_proc_deallocate_fd(proc_ptr, fd_ptr);
	}
}

int vk_fd_table_postpoll_fd(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
	struct vk_io_future *ioft_pre_ptr;
	struct vk_io_future *ioft_post_ptr;
	struct vk_io_future *ioft_ret_ptr;
	struct pollfd pre_event;
	struct pollfd post_event;
	struct pollfd ret_event;

	ioft_pre_ptr  = vk_fd_get_ioft_pre(fd_ptr);
	ioft_post_ptr = vk_fd_get_ioft_post(fd_ptr);
	ioft_ret_ptr  = vk_fd_get_ioft_ret(fd_ptr);
	pre_event  = vk_io_future_get_event(ioft_pre_ptr);
	post_event = vk_io_future_get_event(ioft_post_ptr);
	ret_event  = vk_io_future_get_event(ioft_ret_ptr);

	vk_fd_dbgf("postpoll for FD %i, events %i/%i -> %i/%i -> %i/%i\n", pre_event.fd, pre_event.revents, pre_event.events, post_event.revents, post_event.events, ret_event.revents, ret_event.events);

	if (pre_event.events & post_event.revents) {
		*ioft_ret_ptr = *ioft_post_ptr;
		vk_fd_dbg("triggering processing");
		return 1;
	}


	return 0;
}

#if defined(VK_USE_KQUEUE)
int vk_fd_table_kqueue_kevent(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr, int block) {
	int rc;
	struct timespec timeout;
	int poll_error;
	int i;
	struct vk_fd *fd_ptr;
	struct kevent *ev_ptr;
	int fd;

	/* poll for events */

	timeout.tv_sec = block ? 1 : 0;
	timeout.tv_nsec = 0;

	do {
		vk_kern_receive_signal(kern_ptr);
		DBG("kevent(%i, ..., %i, ..., %i, %i)", fd_table_ptr->kq_fd, fd_table_ptr->kq_nchanges, VK_EV_MAX, block);
		rc = kevent(fd_table_ptr->kq_fd, fd_table_ptr->kq_changelist, fd_table_ptr->kq_nchanges, fd_table_ptr->kq_eventlist, VK_EV_MAX, &timeout);
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
		fd = (int) (intptr_t) ev_ptr->udata;
		fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
		if (fd_ptr == NULL) {
			return -1;
		}
		if (ev_ptr->filter & EVFILT_READ) {
			fd_ptr->ioft_post.event.events  &= ~POLLIN; /* reflect EV_ONESHOT */
			fd_ptr->ioft_post.event.revents |=  POLLIN; /* return event */
			fd_ptr->ioft_post.readable = ev_ptr->data;  /* bytes available to be read on the socket receive queue */
		}
		if (ev_ptr->filter & EVFILT_WRITE) {
			fd_ptr->ioft_post.event.events  &= ~POLLOUT; /* reflect EV_ONESHOT */
			fd_ptr->ioft_post.event.revents |=  POLLOUT; /* return event */
			fd_ptr->ioft_post.writable = ev_ptr->data;   /* bytes available to be written to the socket send queue */
		}
		fd_ptr->ioft_ret = fd_ptr->ioft_post;
		vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
		vk_fd_dbg("dispatching event");
	}

	return 0;
}
int vk_fd_table_kqueue_set(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
	struct kevent *ev_ptr;
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

	need_read        = fd_ptr->ioft_pre.event.events & POLLIN;
	need_write       = fd_ptr->ioft_pre.event.events & POLLOUT;
	registered_read  = fd_ptr->ioft_post.event.events & POLLIN;
	registered_write = fd_ptr->ioft_post.event.events & POLLOUT;
	register_read    = need_read  && ( ! registered_read);
	register_write   = need_write && ( ! registered_write);

	if (register_read) {
		if (fd_table_ptr->kq_nchanges == VK_EV_MAX) {
			errno = ENOBUFS;
			return -1;
		}
		ev_ptr = &fd_table_ptr->kq_changelist[fd_table_ptr->kq_nchanges++];
		ev_ptr->ident = fd_ptr->fd;
		ev_ptr->filter = EVFILT_READ;
		ev_ptr->flags = EV_ADD|EV_ONESHOT;
		ev_ptr->udata = (void *) (intptr_t) fd_ptr->fd;
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
		ev_ptr->flags = EV_ADD|EV_ONESHOT;
		ev_ptr->udata = (void *) (intptr_t) fd_ptr->fd;
	}

	fd_ptr->ioft_post = fd_ptr->ioft_pre;
	vk_fd_dbg("registered event");

	return 0;
}
int vk_fd_table_kqueue_full(struct vk_fd_table *fd_table_ptr) {
	return fd_table_ptr->kq_nchanges == VK_EV_MAX;
}
int vk_fd_table_kqueue(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr) {
	int rc;
	struct vk_fd *fd_ptr;

	if ( ! fd_table_ptr->kq_initialized) {
		rc = kqueue();
		if (rc == -1) {
			return -1;
		}
		fd_table_ptr->kq_fd = rc;
		fd_table_ptr->kq_initialized = 1;
	}

	while ( (fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr)) ) {
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
int vk_fd_table_epoll(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr) {
	int rc;
	struct vk_fd *fd_ptr;
	struct epoll_event ev;
	int fd;
	int ep_op;
	int poll_error;
	int i;

	if ( ! fd_table_ptr->epoll_initialized) {
		rc = epoll_create1(EPOLL_CLOEXEC);
		if (rc == -1) {
			return -1;
		}
		fd_table_ptr->epoll_fd = rc;
		fd_table_ptr->epoll_initialized = 1;
	}

	while ( (fd_ptr = vk_fd_table_dequeue_dirty(fd_table_ptr)) ) {
        if (fd_ptr->closed) {
            fd_ptr->ioft_post.event.events = 0;
            ev.data.fd = fd_ptr->fd;
            ev.events = 0;
			vk_fd_dbg("already closed");
			/* already closed
            ep_op = EPOLL_CTL_DEL;
            DBG("epoll_ctl(%i, %i, %i, [%i, %u])", fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd, ev.data.fd, ev.events);
            rc = epoll_ctl(fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd, &ev);
            DBG(" = %i\n", rc);
            if (rc == -1) {
                PERROR("epoll_ctl");
                return -1;
            }*/
        } else if (fd_ptr->ioft_post.event.events != fd_ptr->ioft_pre.event.events) {
            ev.data.fd = fd_ptr->fd;
            ev.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLET;
            ep_op = EPOLL_CTL_ADD;
            DBG("epoll_ctl(%i, %i, %i, [%i, %u])", fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd, ev.data.fd, ev.events);
            rc = epoll_ctl(fd_table_ptr->epoll_fd, ep_op, fd_ptr->fd, &ev);
            DBG(" = %i\n", rc);
            if (rc == -1) {
                PERROR("epoll_ctl");
                return -1;
            }
	        fd_ptr->ioft_post = fd_ptr->ioft_pre;
		} else {
			vk_fd_dbg("not handled");
		}
	}

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

	for (i = 0; i < fd_table_ptr->epoll_event_count; i++) {
		ev = fd_table_ptr->epoll_events[i];
		fd = ev.data.fd;
		fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
        vk_fd_dbgf("epoll event: ev.data.fd = %i; ev.events = %u\n", ev.data.fd, ev.events);
		if (fd_ptr == NULL) {
			return -1;
		}
        if (ev.events & EPOLLIN) {
            fd_ptr->ioft_post.event.revents |= POLLIN;
			fd_ptr->ioft_post.readable = 1;
        }
        if (ev.events & EPOLLOUT) {
            fd_ptr->ioft_post.event.revents |= POLLOUT;
			fd_ptr->ioft_post.writable = 1;
        }
        if (ev.events & EPOLLHUP) {
            fd_ptr->ioft_post.event.revents |= POLLHUP;
			fd_ptr->ioft_post.writable = 0;
        }
        if (ev.events & EPOLLRDHUP) {
            fd_ptr->ioft_post.event.revents |= POLLERR;
			fd_ptr->ioft_post.readable = 0;
        }
        if (ev.events & EPOLLERR) {
            fd_ptr->ioft_post.event.revents |= POLLERR;
			fd_ptr->ioft_post.readable = 0;
			fd_ptr->ioft_post.writable = 1;
        }
        fd_ptr->ioft_ret = fd_ptr->ioft_post;
		vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
	}

	return 0;
}
#else
#endif
int vk_fd_table_poll(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr) {
	int rc;
	struct vk_fd *fd_ptr;
	nfds_t i;
	int fd;
	int poll_error;

	fd_table_ptr->poll_nfds = 0;
	fd_ptr = vk_fd_table_first_dirty(fd_table_ptr);
	while (fd_ptr && fd_table_ptr->poll_nfds < VK_FD_MAX) {
		fd_ptr->ioft_post = fd_ptr->ioft_pre;
		fd_table_ptr->poll_fds[fd_table_ptr->poll_nfds++] = fd_ptr->ioft_post.event;

		fd_ptr = vk_fd_next_dirty_fd(fd_ptr);
	}

	do {
		vk_kern_receive_signal(kern_ptr);
		DBG("poll(..., %li, 1000)", fd_table_ptr->poll_nfds);
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

	for (i = 0; i < fd_table_ptr->poll_nfds; i++) {
		fd = fd_table_ptr->poll_fds[i].fd;
		fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
		if (fd_ptr == NULL) {
			return -1;
		}
		fd_ptr->ioft_post.event = fd_table_ptr->poll_fds[i];
		if (fd_ptr->ioft_post.event.events & POLLIN) {
			fd_ptr->ioft_post.readable = 1;
		}
		if (fd_ptr->ioft_post.event.events & POLLOUT) {
			fd_ptr->ioft_post.writable = 1;
		}
		if (fd_ptr->ioft_post.event.events & POLLHUP) {
			fd_ptr->ioft_post.writable = 1;
		}
		if (fd_ptr->ioft_post.event.events & POLLERR) {
			fd_ptr->ioft_post.readable = 0;
			fd_ptr->ioft_post.writable = 0;
		}
		fd_ptr->ioft_ret = fd_ptr->ioft_post;
		vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
		if (fd_ptr->ioft_pre.event.events & fd_ptr->ioft_ret.event.revents) {
			vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
		}
	}

	return 0;
}

int vk_fd_table_wait(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr) {
	if (fd_table_ptr->poll_driver == VK_POLL_DRIVER_POLL) {
		return vk_fd_table_poll(fd_table_ptr, kern_ptr);
	} else {
#if defined(VK_USE_KQUEUE)
		return vk_fd_table_kqueue(fd_table_ptr, kern_ptr);
#elif defined(VK_USE_EPOLL)
		return vk_fd_table_epoll(fd_table_ptr, kern_ptr);
#else
#endif
	}
}
