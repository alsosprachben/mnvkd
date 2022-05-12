#ifndef VK_POLL_H
#define VK_POLL_H

#include "vk_socket.h"
struct io_future;

void io_future_init(struct io_future *ioft, struct vk_socket *socket_ptr);

#endif