#ifndef VK_FD_TABLE_H
#define VK_FD_TABLE_H

#include <stddef.h>
struct vk_fd_table;
struct vk_fd;
struct vk_socket;
struct vk_kern;
struct vk_proc;
struct vk_io_future;

enum vk_poll_driver {
	VK_POLL_DRIVER_POLL = 0, /* use posix poll() interface */
	VK_POLL_DRIVER_OS   = 1, /* use os-specific kqueue() or epoll() interface */
};

enum vk_poll_method {
	VK_POLL_METHOD_LEVEL_TRIGGERED_ONESHOT = 0, /* register whenever there is a block (poll() and kqueue() get this for free due to registration batching, but epoll() incurs extra system calls) */
	VK_POLL_METHOD_EDGE_TRIGGERED          = 1, /* register only once (if VK_POLL_DRIVER_OS and OS == linux, then use edge-triggered epoll()) */
};

size_t vk_fd_table_alloc_size(size_t size);

enum vk_poll_driver vk_fd_table_get_poll_driver(struct vk_fd_table *fd_table_ptr);
void vk_fd_table_set_poll_driver(struct vk_fd_table *fd_table_ptr, enum vk_poll_driver poll_driver);

enum vk_poll_method vk_fd_table_get_poll_method(struct vk_fd_table *fd_table_ptr);
void vk_fd_table_set_poll_method(struct vk_fd_table *fd_table_ptr, enum vk_poll_method poll_method);

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

void vk_fd_table_dump(struct vk_fd_table *fd_table_ptr);

void vk_fd_table_prepoll_blocked_fd(struct vk_fd_table *fd_table_ptr, struct vk_socket *socket_ptr, struct vk_io_future *ioft_ptr, struct vk_proc *proc_ptr);
void vk_fd_table_prepoll_blocked_socket(struct vk_fd_table *fd_table_ptr, struct vk_socket *socket_ptr, struct vk_proc *proc_ptr);
void vk_fd_table_prepoll_enqueue_closed_fd(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr);
void vk_fd_table_prepoll_zombie(struct vk_fd_table *fd_table_ptr, struct vk_proc *proc_ptr);
int vk_fd_table_postpoll_fd(struct vk_fd_table *fd_table_ptr, struct vk_fd *fd_ptr);

int vk_fd_table_wait(struct vk_fd_table *fd_table_ptr, struct vk_kern *kern_ptr);

#endif
