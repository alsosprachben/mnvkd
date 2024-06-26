#include "vk_fd.h"
#include "vk_fd_s.h"

#include "vk_io_future.h"
#include "vk_kern.h"
#include "vk_socket.h"
#include "vk_thread.h"

int vk_fd_get_fd(struct vk_fd* fd_ptr) { return fd_ptr->fd; }
void vk_fd_set_fd(struct vk_fd* fd_ptr, int fd) { fd_ptr->fd = fd; }

size_t vk_fd_get_proc_id(struct vk_fd* fd_ptr) { return fd_ptr->proc_id; }
void vk_fd_set_proc_id(struct vk_fd* fd_ptr, size_t proc_id) { fd_ptr->proc_id = proc_id; }

int vk_fd_get_allocated(struct vk_fd* fd_ptr) { return fd_ptr->allocated; }
void vk_fd_set_allocated(struct vk_fd* fd_ptr, int allocated) { fd_ptr->allocated = allocated; }
int vk_fd_allocate(struct vk_fd* fd_ptr, int fd, size_t proc_id)
{
	if (fd_ptr->allocated) {
		errno = ENOENT;
		return -1;
	} else {
		fd_ptr->fd = fd;
		fd_ptr->proc_id = proc_id;
		fd_ptr->allocated = 1;
		fd_ptr->closed = 0;
		fd_ptr->added = 0;
		fd_ptr->zombie = 0;
		return 0;
	}
}
void vk_fd_free(struct vk_fd* fd_ptr)
{
	fd_ptr->allocated = 0;
	fd_ptr->closed = 0;
	fd_ptr->added = 0;
	fd_ptr->zombie = 0;
}

struct vk_fd* vk_fd_next_allocated_fd(struct vk_fd* fd_ptr) { return SLIST_NEXT(fd_ptr, allocated_list_elem); }

int vk_fd_get_zombie(struct vk_fd* fd_ptr) { return fd_ptr->zombie; }
void vk_fd_set_zombie(struct vk_fd* fd_ptr, int zombie) { fd_ptr->zombie = zombie; }

int vk_fd_get_closed(struct vk_fd* fd_ptr) { return fd_ptr->closed; }
void vk_fd_set_closed(struct vk_fd* fd_ptr, int closed) { fd_ptr->closed = closed; }

int vk_fd_get_error(struct vk_fd* fd_ptr) { return fd_ptr->error; }
void vk_fd_set_error(struct vk_fd* fd_ptr, int error) { fd_ptr->error = error; }

struct vk_io_future* vk_fd_get_ioft_post(struct vk_fd* fd_ptr) { return &fd_ptr->ioft_post; }
void vk_fd_set_ioft_post(struct vk_fd* fd_ptr, struct vk_io_future* ioft_ptr) { fd_ptr->ioft_post = *ioft_ptr; }

struct vk_io_future* vk_fd_get_ioft_pre(struct vk_fd* fd_ptr) { return &fd_ptr->ioft_pre; }
void vk_fd_set_ioft_pre(struct vk_fd* fd_ptr, struct vk_io_future* ioft_ptr) { fd_ptr->ioft_pre = *ioft_ptr; }

struct vk_io_future* vk_fd_get_ioft_ret(struct vk_fd* fd_ptr) { return &fd_ptr->ioft_ret; }
void vk_fd_set_ioft_ret(struct vk_fd* fd_ptr, struct vk_io_future* ioft_ptr) { fd_ptr->ioft_ret = *ioft_ptr; }

struct vk_fd* vk_fd_next_dirty_fd(struct vk_fd* fd_ptr) { return SLIST_NEXT(fd_ptr, dirty_list_elem); }

struct vk_fd* vk_fd_next_fresh_fd(struct vk_fd* fd_ptr) { return SLIST_NEXT(fd_ptr, fresh_list_elem); }

enum vk_fd_type vk_fd_get_type(struct vk_fd* fd_ptr) { return fd_ptr->type; }
void vk_fd_set_type(struct vk_fd* fd_ptr, enum vk_fd_type type) { fd_ptr->type = type; }
