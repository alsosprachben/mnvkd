#ifndef VK_IO_FUTURE_H
#define VK_IO_FUTURE_H

struct vk_io_future;
struct vk_socket;

#include <poll.h>
#include <stdint.h>

struct vk_socket *vk_io_future_get_socket(struct vk_io_future *ioft_ptr);
void vk_io_future_set_socket(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr);

struct pollfd vk_io_future_get_event(struct vk_io_future *ioft_ptr);
void vk_io_future_set_event(struct vk_io_future *ioft_ptr, struct pollfd event);

intptr_t vk_io_future_get_data(struct vk_io_future *ioft_ptr);
void vk_io_future_set_data(struct vk_io_future *ioft_ptr, intptr_t data);

int vk_io_future_get_closed(struct vk_io_future *ioft_ptr);
void vk_io_future_set_closed(struct vk_io_future *ioft_ptr, int closed);

void vk_io_future_init(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr);

#endif