#ifndef VK_SOCKET_S_H
#define VK_SOCKET_S_H

#include "vk_queue.h"

#include "vk_pipe.h"
#include "vk_socket.h"

#include "vk_io_future_s.h"
#include "vk_pipe_s.h"
#include "vk_vectoring_s.h"

struct vk_buffering {
	char buf[4096 * 4];
	struct vk_vectoring ring;
};
#define VK_BUFFERING_INIT(buffering) VK_VECTORING_INIT(&((buffering).ring), (buffering).buf)

struct vk_block {
	int op;
	char* buf;
	size_t len;
	size_t copied;
	ssize_t rc;

	/* state to register:          logical,  prior */
	struct vk_io_future ioft_rx_pre;
	struct vk_io_future ioft_tx_pre;
	/* state to dispatch:          logical,  posterior */
	struct vk_io_future ioft_rx_ret;
	struct vk_io_future ioft_tx_ret;

	struct vk_thread* blocked_vk;
};

#define VK_BLOCK_INIT(block, socket_arg, blocked_vk_arg)                                                               \
	do {                                                                                                           \
		(block).op = 0;                                                                                        \
		(block).buf = NULL;                                                                                    \
		(block).len = 0;                                                                                       \
		(block).rc = 0;                                                                                        \
		vk_io_future_init(&(block).ioft_rx_pre, &(socket_arg));                                                \
		vk_io_future_init(&(block).ioft_tx_ret, &(socket_arg));                                                \
		vk_io_future_init(&(block).ioft_rx_pre, &(socket_arg));                                                \
		vk_io_future_init(&(block).ioft_tx_ret, &(socket_arg));                                                \
		(block).blocked_vk = (blocked_vk_arg);                                                                 \
	} while (0)

struct vk_socket {
	struct vk_buffering rx;
	struct vk_buffering tx;
	struct vk_block block;
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	int error; /* `errno` via socket ops, forwarded from `struct vk_vectoring` member `error` */

	/* distinct socket blocked queue for local process -- head on `struct vk_proc_local` at `blocked_q` */
	SLIST_ENTRY(vk_socket) blocked_q_elem;
	int blocked_enq; /* to make entries distinct */
};

#define VK_SOCKET_INIT(socket, blocked_vk_arg, rx_fd_arg, tx_fd_arg)                                                   \
	{                                                                                                              \
		VK_BUFFERING_INIT((socket).rx);                                                                        \
		VK_BUFFERING_INIT((socket).tx);                                                                        \
		VK_BLOCK_INIT((socket).block, (socket), (blocked_vk_arg));                                             \
		(socket).rx_fd = (rx_fd_arg);                                                                          \
		(socket).tx_fd = (tx_fd_arg);                                                                          \
		(socket).blocked_q_elem.sle_next = NULL;                                                               \
		(socket).blocked_enq = 0;                                                                              \
	}                                                                                                              \
	while (0)

#endif