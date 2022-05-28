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
void vk_block_init(struct vk_block *block, char *buf, size_t len, int op);
ssize_t vk_block_commit(struct vk_block *block, ssize_t rc);
size_t vk_block_get_committed(struct vk_block *block);
size_t vk_block_get_uncommitted(struct vk_block *block);
void vk_block_set_uncommitted(struct vk_block *block_ptr, size_t len);
char *vk_block_get_buf(struct vk_block *block_ptr);

struct vk_socket;

void vk_socket_init(struct vk_socket *socket_ptr, struct that *that, struct vk_pipe rx, struct vk_pipe tx);
struct vk_vectoring *vk_socket_get_rx_vectoring(struct vk_socket *socket_ptr);
struct vk_vectoring *vk_socket_get_tx_vectoring(struct vk_socket *socket_ptr);
struct vk_block *vk_socket_get_block(struct vk_socket *socket_ptr);
void vk_socket_enqueue_blocked(struct vk_socket *socket_ptr);
int vk_socket_get_enqueued_blocked(struct vk_socket *socket_ptr);
void vk_socket_set_enqueued_blocked(struct vk_socket *socket_ptr, int blocked_enq);

/* satisfy VK_OP_READ */
ssize_t vk_socket_read(struct vk_socket *socket);
/* satisfy VK_OP_WRITE */
ssize_t vk_socket_write(struct vk_socket *socket);
/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket);

#endif
