#include "vk_fd.h"
#include "vk_fd_s.h"

#include "vk_socket.h"
#include "vk_thread.h"

/* vk_io_future */
struct vk_socket *vk_io_future_get_socket(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->socket_ptr;
}
void vk_io_future_set_socket(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr) {
	ioft_ptr->socket_ptr = socket_ptr;
}

struct pollfd vk_io_future_get_event(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->event;
}
void vk_io_future_set_event(struct vk_io_future *ioft_ptr, struct pollfd event) {
	ioft_ptr->event = event;
}

intptr_t vk_io_future_get_data(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->data;
}
void vk_io_future_set_data(struct vk_io_future *ioft_ptr, intptr_t data) {
	ioft_ptr->data = data;
}

void vk_io_future_init(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr) {
	ioft_ptr->socket_ptr = socket_ptr;
	ioft_ptr->event.fd      = vk_socket_get_blocked_fd(socket_ptr);
	ioft_ptr->event.events  = vk_socket_get_blocked_events(socket_ptr);
	ioft_ptr->event.revents = 0;
	ioft_ptr->data = 0;
}

/* vk_fd */
int vk_fd_get_fd(struct vk_fd *fd_ptr) {
	return fd_ptr->fd;
}
void vk_fd_set_fd(struct vk_fd *fd_ptr, int fd) {
	fd_ptr->fd = fd;
}

size_t vk_fd_get_proc_id(struct vk_fd *fd_ptr) {
	return fd_ptr->proc_id;
}
void vk_fd_set_proc_id(struct vk_fd *fd_ptr, size_t proc_id) {
	fd_ptr->proc_id = proc_id;
}

int vk_fd_get_allocated(struct vk_fd *fd_ptr) {
	return fd_ptr->allocated;
}
void vk_fd_set_allocated(struct vk_fd *fd_ptr, int allocated) {
	fd_ptr->allocated = allocated;
}
int vk_fd_allocate(struct vk_fd *fd_ptr, int fd, size_t proc_id) {
	if (fd_ptr->allocated) {
		errno = ENOENT;
		return -1;
	} else {
		fd_ptr->fd = fd;
		fd_ptr->proc_id = proc_id;
		fd_ptr->allocated = 1;
		return 0;
	}
}
void vk_fd_free(struct vk_fd *fd_ptr) {
	fd_ptr->allocated = 0;
}

struct vk_io_future *vk_fd_get_ioft_post(struct vk_fd *fd_ptr) {
	return &fd_ptr->ioft_post;
}
void vk_fd_set_ioft_post(struct vk_fd *fd_ptr, struct vk_io_future *ioft_ptr) {
	fd_ptr->ioft_post = *ioft_ptr;
}

struct vk_io_future *vk_fd_get_ioft_pre(struct vk_fd *fd_ptr) {
	return &fd_ptr->ioft_pre;
}
void vk_fd_set_ioft_pre(struct vk_fd *fd_ptr, struct vk_io_future *ioft_ptr) {
	fd_ptr->ioft_pre = *ioft_ptr;
}

struct vk_fd *vk_fd_next_run_proc(struct vk_fd *fd_ptr) {
    return SLIST_NEXT(fd_ptr, dirty_list_elem);
}

struct vk_fd *vk_fd_next_blocked_proc(struct vk_fd *fd_ptr) {
    return SLIST_NEXT(fd_ptr, fresh_list_elem);
}

/* vk_fd_table */

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
		vk_socket_dbg("Socket is not blocked on an FD, so nothing to poll for it.");
		return;
	}

	fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
	vk_fd_set_fd(fd_ptr, fd);
	vk_fd_set_proc_id(fd_ptr, proc_id);
	ioft_ptr = vk_fd_get_ioft_pre(fd_ptr);
	vk_io_future_init(ioft_ptr, socket_ptr);
	event = vk_io_future_get_event(ioft_ptr);
	vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);

	vk_socket_dbgf("prepoll for pid %zu, FD %i, events %i\n", proc_id, event.fd, event.events);

	return;
}

int vk_fd_table_postpoll(struct vk_fd_table *fd_table_ptr, struct vk_socket *socket_ptr, size_t proc_id) {
	struct vk_fd *fd_ptr;
	struct vk_io_future *ioft_pre_ptr;
	struct vk_io_future *ioft_post_ptr;
	struct pollfd event;
	int fd;
	int blocked_events;

	fd = vk_socket_get_blocked_fd(socket_ptr);
	if (fd == -1) {
		vk_socket_dbg("Socket does not have FD, so do not poll it.");
		return 0;
	}

	fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
	vk_fd_set_fd(fd_ptr, fd);
	vk_fd_set_proc_id(fd_ptr, proc_id);
	ioft_pre_ptr = vk_fd_get_ioft_pre(fd_ptr);
	ioft_post_ptr = vk_fd_get_ioft_post(fd_ptr);

	blocked_events = vk_socket_get_blocked_events(socket_ptr);
	if (event.events & blocked_events) {
		return 1; /* trigger processing */
	}

	vk_socket_dbgf("prepoll for pid %zu, FD %i, events %i\n", proc_id, event.fd, event.events);

	return 0;
}

int vk_fd_table_poll(struct vk_fd_table *fd_table_ptr) {
	return 0;
}