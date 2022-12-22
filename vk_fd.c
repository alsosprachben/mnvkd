#include "vk_fd.h"
#include "vk_fd_s.h"

#include "vk_socket.h"
#include "vk_thread.h"
#include "vk_kern.h"
#include "vk_io_future.h"

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

struct vk_fd *vk_fd_next_dirty_fd(struct vk_fd *fd_ptr) {
    return SLIST_NEXT(fd_ptr, dirty_list_elem);
}

struct vk_fd *vk_fd_next_fresh_fd(struct vk_fd *fd_ptr) {
    return SLIST_NEXT(fd_ptr, fresh_list_elem);
}



