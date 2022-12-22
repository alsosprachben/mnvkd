#include "vk_debug.h"
#include "vk_fd_table.h"
#include "vk_fd_table_s.h"
#include "vk_io_future.h"
#include "vk_kern.h"

#include <stddef.h>
#include <errno.h>
#include <sys/epoll.h>

size_t vk_fd_table_alloc_size(size_t size) {
	return sizeof (struct vk_fd_table) + (sizeof (struct vk_fd) * size);
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


void vk_fd_table_prepoll(struct vk_fd_table *fd_table_ptr, struct vk_socket *socket_ptr, size_t proc_id) {
	struct vk_fd *fd_ptr;
	struct vk_io_future *ioft_ptr;
	struct pollfd event;
	int fd;

	fd = vk_socket_get_blocked_fd(socket_ptr);
	if (fd == -1) {
		vk_socket_dbgf("prepoll for pid %zu, socket is not blocked on FD, so nothing to poll for it.\n", proc_id);
		return;
	}

	fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
	if (fd_ptr == NULL) {
		return;
	}
	vk_fd_set_fd(fd_ptr, fd);
	vk_fd_set_proc_id(fd_ptr, proc_id);
	ioft_ptr = vk_fd_get_ioft_pre(fd_ptr);
	vk_io_future_init(ioft_ptr, socket_ptr);
	event = vk_io_future_get_event(ioft_ptr);
	vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);

	vk_socket_dbgf("prepoll for pid %zu, FD %i, events %i\n", proc_id, event.fd, event.events);

	return;
}

int vk_fd_table_postpoll(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr) {
	struct vk_io_future *ioft_pre_ptr;
	struct pollfd event;

	ioft_pre_ptr = vk_fd_get_ioft_pre(fd_ptr);
	event = vk_io_future_get_event(ioft_pre_ptr);

	vk_fd_dbgf("postpoll for FD %i, events %i/%i\n", event.fd, event.events, event.revents);

	if (event.events & event.revents) {
		return 1; /* trigger processing */
	}


	return 0;
}

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
		fd_ptr->ioft_pre = fd_ptr->ioft_post;
		fd_ptr->ioft_pre.event = fd_table_ptr->poll_fds[i];
		vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
		if (fd_ptr->ioft_pre.event.events & fd_ptr->ioft_pre.event.revents) {
			vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
		}
	}

	return 0;
}

#ifdef __linux__
int vk_fd_table_epoll(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr) {
	int rc;
	struct vk_fd *fd_ptr;
	struct epoll_event ev;
	int fd;
	int ep_op;
	int poll_error;
	int i;

	if ( ! fd_table_ptr->epoll_initialized) {
		fd_table_ptr->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
		fd_table_ptr->epoll_initialized = 1;
	}

	fd_ptr = vk_fd_table_first_dirty(fd_table_ptr);
	while (fd_ptr && fd_table_ptr->poll_nfds < VK_FD_MAX) {
		ev.data.fd = fd_ptr->ioft_post.event.fd;
		ev.events = fd_ptr->ioft_post.event.events|EPOLLET|EPOLLRDHUP;
		if (fd_ptr->ioft_post.event.events == 0) {
			ep_op = EPOLL_CTL_ADD;
		} else {
			ep_op = EPOLL_CTL_MOD;
		}
		rc = epoll_ctl(fd_table_ptr->epoll_fd, ep_op, fd_ptr->ioft_post.event.fd, &ev);
		if (rc == -1) {
			PERROR("epoll_ctl");
			return -1;
		}

		fd_ptr->ioft_post = fd_ptr->ioft_pre;

		fd_ptr = vk_fd_next_dirty_fd(fd_ptr);

		vk_fd_table_drop_dirty(fd_table_ptr, fd_ptr);
	}

	do {
		vk_kern_receive_signal(kern_ptr);
		DBG("epoll_wait(..., %i, 1000)", fd_table_ptr->epoll_event_count);
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
		if (fd_ptr == NULL) {
			return -1;
		}
		fd_ptr->ioft_pre = fd_ptr->ioft_post;
		fd_ptr->ioft_pre.event.revents = ev.events;
		vk_fd_table_enqueue_fresh(fd_table_ptr, fd_ptr);
	}

	return 0;
}

#endif