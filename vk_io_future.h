#ifndef VK_IO_FUTURE_H
#define VK_IO_FUTURE_H

struct vk_io_future;
struct vk_socket;

#include <poll.h>
#include <unistd.h>

struct vk_socket *vk_io_future_get_socket(struct vk_io_future *ioft_ptr);
void vk_io_future_set_socket(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr);

struct pollfd vk_io_future_get_event(struct vk_io_future *ioft_ptr);
void vk_io_future_set_event(struct vk_io_future *ioft_ptr, struct pollfd event);

size_t vk_io_future_get_readable(struct vk_io_future *ioft_ptr);
void vk_io_future_set_readable(struct vk_io_future *ioft_ptr, size_t readable);

size_t vk_io_future_get_writable(struct vk_io_future *ioft_ptr);
void vk_io_future_set_writable(struct vk_io_future *ioft_ptr, size_t writable);

int vk_io_future_get_closed(struct vk_io_future *ioft_ptr);
void vk_io_future_set_closed(struct vk_io_future *ioft_ptr, int closed);

void vk_io_future_init(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr);

#endif
