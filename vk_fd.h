#ifndef VK_FD_H
#define VK_FD_H

#include "vk_socket.h"

struct vk_io_future;
struct vk_fd;
struct vk_fd_table;

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

struct vk_io_future *vk_fd_get_ioft_post(struct vk_fd *fd_ptr);
void vk_fd_set_ioft_post(struct vk_fd *fd_ptr, struct vk_io_future *ioft_ptr);

struct vk_io_future *vk_fd_get_ioft_pre(struct vk_fd *fd_ptr);
void vk_fd_set_ioft_pre(struct vk_fd *fd_ptr, struct vk_io_future *ioft_ptr);

struct vk_fd *vk_fd_next_dirty_fd(struct vk_fd *fd_ptr);
struct vk_fd *vk_fd_next_fresh_fd(struct vk_fd *fd_ptr);

size_t vk_fd_table_alloc_size(size_t size);

size_t vk_fd_table_get_size(struct vk_fd_table *fd_table_ptr);
void vk_fd_table_set_size(struct vk_fd_table *fd_table_ptr, size_t size);

struct vk_fd *vk_fd_table_get(struct vk_fd_table *fd_table_ptr, size_t i);

struct vk_fd *vk_fd_table_first_dirty(struct vk_fd_table *fd_table_ptr);
struct vk_fd *vk_fd_table_first_fresh(struct vk_fd_table *fd_table_ptr);
void vk_fd_table_enqueue_dirty(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr);
void vk_fd_table_enqueue_fresh(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr);
void vk_fd_table_drop_dirty(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr);
void vk_fd_table_drop_fresh(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr);
struct vk_fd *vk_fd_table_dequeue_dirty(struct vk_fd_table *fd_table_ptr);
struct vk_fd *vk_fd_table_dequeue_fresh(struct vk_fd_table *fd_table_ptr);

void vk_fd_table_prepoll(struct vk_fd_table *fd_table_ptr, struct vk_socket *socket_ptr, size_t proc_id);
int vk_fd_table_postpoll(struct vk_fd_table *fd_table_ptr, struct vk_socket *socket_ptr);

#endif
