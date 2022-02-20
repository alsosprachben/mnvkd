#ifndef VK_SOCKET_H
#define VK_SOCKET_H

#include "vk_vectoring.h"

struct vk_buffering {
	char buf[4096 * 4];
	struct vk_vectoring ring;
};

#define VK_BUFFERING_INIT(buffering) VK_VECTORING_INIT(&((buffering).ring), (buffering).buf)

#define VK_OP_READ 1
#define VK_OP_WRITE 2
struct vk_block {
	int op;
	char *buf;
	size_t len;
	size_t copied;
	ssize_t rc;
};

#define VK_BLOCK_INIT(block) { \
	(block).op  = 0; \
	(block).buf = NULL; \
	(block).len = 0; \
	(block).rc  = 0; \
}

struct vk_socket {
	struct vk_buffering rx;
	struct vk_buffering tx;
	struct vk_block block;
	int rx_fd;
	int tx_fd;
	int error; /* errno */
};

#define VK_SOCKET_INIT(socket, rx_fd_arg, tx_fd_arg) { \
	VK_BUFFERING_INIT((socket).rx); \
	VK_BUFFERING_INIT((socket).tx); \
	VK_BLOCK_INIT((socket).block); \
	(socket).rx_fd = (rx_fd_arg); \
	(socket).tx_fd = (tx_fd_arg); \
	(socket).error = 0; \
}

/* satisfy VK_OP_READ */
ssize_t vk_socket_read(struct vk_socket *socket);
/* satisfy VK_OP_WRITE */
ssize_t vk_socket_write(struct vk_socket *socket);
/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket);

#endif
