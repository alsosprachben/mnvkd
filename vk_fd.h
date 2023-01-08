#ifndef VK_FD_H
#define VK_FD_H

#include <stddef.h>
#include <stdint.h>

struct vk_socket;
struct vk_io_future;
struct vk_fd;
struct vk_fd_table;
struct vk_kern;

struct vk_socket *vk_io_future_get_socket(struct vk_io_future *ioft_ptr);
void vk_io_future_set_socket(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr);

struct pollfd vk_io_future_get_event(struct vk_io_future *ioft_ptr);
void vk_io_future_set_event(struct vk_io_future *ioft_ptr, struct pollfd event);

intptr_t vk_io_future_get_data(struct vk_io_future *ioft_ptr);
void vk_io_future_set_data(struct vk_io_future *ioft_ptr, intptr_t data);

void vk_io_future_init(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr);

int vk_fd_get_fd(struct vk_fd *fd_ptr);
void vk_fd_set_fd(struct vk_fd *fd_tr, int fd);

size_t vk_fd_get_proc_id(struct vk_fd *fd_ptr);
void vk_fd_set_proc_id(struct vk_fd *fd_ptr, size_t proc_id);

int vk_fd_get_allocated(struct vk_fd *fd_ptr);
void vk_fd_set_allocated(struct vk_fd *fd_ptr, int allocated);
int vk_fd_allocate(struct vk_fd *fd_ptr, int fd, size_t proc_id);
void vk_fd_free(struct vk_fd *fd_ptr);

int vk_fd_get_closed(struct vk_fd *fd_ptr);
void vk_fd_set_closed(struct vk_fd *fd_ptr, int closed);

struct vk_fd *vk_fd_next_allocated_fd(struct vk_fd *fd_ptr);

int vk_fd_get_error(struct vk_fd *fd_ptr);
void vk_fd_set_error(struct vk_fd *fd_ptr, int error);

struct vk_io_future *vk_fd_get_ioft_post(struct vk_fd *fd_ptr);
void vk_fd_set_ioft_post(struct vk_fd *fd_ptr, struct vk_io_future *ioft_ptr);

struct vk_io_future *vk_fd_get_ioft_pre(struct vk_fd *fd_ptr);
void vk_fd_set_ioft_pre(struct vk_fd *fd_ptr, struct vk_io_future *ioft_ptr);

struct vk_io_future *vk_fd_get_ioft_ret(struct vk_fd *fd_ptr);
void vk_fd_set_ioft_ret(struct vk_fd *fd_ptr, struct vk_io_future *ioft_ptr);

struct vk_fd *vk_fd_next_dirty_fd(struct vk_fd *fd_ptr);
struct vk_fd *vk_fd_next_fresh_fd(struct vk_fd *fd_ptr);

#endif
