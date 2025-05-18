#ifndef VK_SOCKET_S_H
#define VK_SOCKET_S_H

#include "vk_queue.h"

#include "vk_pipe.h"
#include "vk_socket.h"

#include "vk_io_future_s.h"
#include "vk_pipe_s.h"
#include "vk_vectoring_s.h"
#ifdef USE_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>

struct vk_tls_ssl {
	SSL* ssl_ptr;

#define VK_TLS_HANDSHAKE_INIT 0
#define VK_TLS_HANDSHAKE_IN_PROGRESS 1
#define VK_TLS_HANDSHAKE_COMPLETE 2
	int handshake_phase;
}
#endif

struct vk_buffering {
	char buf[4096 * 4];
	struct vk_vectoring ring;
};

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

#ifdef USE_TLS
	struct vk_tls_ssl tls; /* TLS state */
#endif
};

#endif