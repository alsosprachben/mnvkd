#ifndef VK_SOCKET_H
#define VK_SOCKET_H

#include "vk_vectoring.h"
#include "vk_pipe.h"

struct that;   /* forward declare coroutine */
struct socket; /* forward declare socket */

#include "vk_vectoring_s.h"

struct vk_buffering;

enum vk_op_type {
	VK_OP_READ,
	VK_OP_WRITE,
	VK_OP_FLUSH,
};
struct vk_block;

struct vk_socket;

void vk_socket_init(struct vk_socket *socket_ptr, struct that *that, struct vk_pipe rx, struct vk_pipe tx);

/* satisfy VK_OP_READ */
ssize_t vk_socket_read(struct vk_socket *socket);
/* satisfy VK_OP_WRITE */
ssize_t vk_socket_write(struct vk_socket *socket);
/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket);

#endif
