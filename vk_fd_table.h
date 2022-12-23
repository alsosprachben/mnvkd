#ifndef VK_FD_TABLE_H
#define VK_FD_TABLE_H

#include <stddef.h>
struct vk_fd_table;
struct vk_fd;
struct vk_socket;
struct vk_kern;

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
int vk_fd_table_postpoll(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr);

int vk_fd_table_wait(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr);

#endif
