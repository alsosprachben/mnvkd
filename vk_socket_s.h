#ifndef VK_SOCKET_S_H
#define VK_SOCKET_S_H

#include "vk_socket.h"
#include "vk_pipe.h"

#include "vk_vectoring_s.h"

struct vk_buffering {
	char buf[4096 * 4];
	struct vk_vectoring ring;
};
#define VK_BUFFERING_INIT(buffering) VK_VECTORING_INIT(&((buffering).ring), (buffering).buf)

struct vk_block {
	int op;
	char *buf;
	size_t len;
	size_t copied;
	ssize_t rc;
	int blocked;
	int blocked_fd;
	struct that *blocked_vk;
};

#define VK_BLOCK_INIT(block, blocked_vk_arg) { \
	(block).op  = 0; \
	(block).buf = NULL; \
	(block).len = 0; \
	(block).rc  = 0; \
	(block).blocked = 0; \
	(block).blocked_fd = -1; \
	(block).blocked_vk = blocked_vk_arg; \
}

struct vk_socket {
	struct vk_buffering rx;
	struct vk_buffering tx;
	struct vk_block block;
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	int error; /* errno */
};

#define VK_SOCKET_INIT(socket, blocked_vk_arg, rx_fd_arg, tx_fd_arg) { \
	VK_BUFFERING_INIT((socket).rx); \
	VK_BUFFERING_INIT((socket).tx); \
	VK_BLOCK_INIT((socket).block, blocked_vk_arg); \
	(socket).rx_fd = (rx_fd_arg); \
	(socket).tx_fd = (tx_fd_arg); \
	(socket).error = 0; \
}

void vk_socket_init(struct vk_socket *socket_ptr, struct that *that, struct vk_pipe rx, struct vk_pipe tx) {
    VK_SOCKET_INIT(*socket_ptr, that, rx, tx);
}

#endif