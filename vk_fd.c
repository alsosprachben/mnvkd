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
	switch (vk_block_get_op(vk_socket_get_block(socket_ptr))) {
		case VK_OP_READ:
		case VK_OP_READABLE:
			ioft_ptr->event.fd = vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr));
			ioft_ptr->event.events = POLLIN;
			ioft_ptr->event.revents = 0;
			break;
		case VK_OP_WRITE:
		case VK_OP_FLUSH:
		case VK_OP_WRITABLE:
			ioft_ptr->event.fd = vk_pipe_get_fd(vk_socket_get_tx_fd(socket_ptr));
			ioft_ptr->event.events = POLLOUT;
			ioft_ptr->event.revents = 0;
			break;
	}
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

/* vk_fd_table */


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
