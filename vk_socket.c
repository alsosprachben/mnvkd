#include "vk_state.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "debug.h"

/* satisfy VK_OP_READ */
ssize_t vk_socket_handle_read(struct vk_socket *socket) {
	switch (socket->rx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_vectoring_read(&socket->rx.ring, VK_PIPE_GET_FD(socket->rx_fd));
			if (vk_vectoring_has_effect(&socket->rx.ring)) {
				vk_vectoring_clear_effect(&socket->rx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
			}
			socket->block.blocked = vk_vectoring_rx_is_blocked(&socket->rx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->rx_fd);
			break;
		case VK_PIPE_VK_RX:
			vk_vectoring_splice(&socket->rx.ring, VK_PIPE_GET_RX(socket->rx_fd));
			if (vk_vectoring_has_effect(&socket->rx.ring)) {
				vk_vectoring_clear_effect(&socket->rx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk);
				vk_enqueue_run(VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_rx_is_blocked(&socket->rx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->rx_fd);
			break;
		case VK_PIPE_VK_TX:
			vk_vectoring_splice(&socket->rx.ring, VK_PIPE_GET_TX(socket->rx_fd));
			if (vk_vectoring_has_effect(&socket->rx.ring)) {
				vk_vectoring_clear_effect(&socket->rx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk);
				vk_enqueue_run(VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_rx_is_blocked(&socket->rx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->rx_fd);
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket->tx.ring.error != 0) {
		socket->error = socket->rx.ring.error;
	}
	return 0;
}

/* satisfy VK_OP_WRITE */
ssize_t vk_socket_handle_write(struct vk_socket *socket) {
	switch (socket->tx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_vectoring_write(&socket->tx.ring, VK_PIPE_GET_FD(socket->tx_fd));
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->tx_fd);
			break;
		case VK_PIPE_VK_RX:
			vk_vectoring_splice(VK_PIPE_GET_RX(socket->tx_fd), &socket->tx.ring);
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk);
				vk_enqueue_run(VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->tx_fd);
			break;
		case VK_PIPE_VK_TX:
			vk_vectoring_splice(VK_PIPE_GET_TX(socket->tx_fd), &socket->tx.ring);
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk);
				vk_enqueue_run(VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->tx_fd);
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket->tx.ring.error != 0) {
		socket->error = socket->tx.ring.error;
	}
	return 0;
}

void vk_socket_init(struct vk_socket *socket_ptr, struct that *that, struct vk_pipe rx, struct vk_pipe tx) {
    VK_SOCKET_INIT(*socket_ptr, that, rx, tx);
}

void vk_socket_enqueue_blocked(struct vk_socket *socket_ptr) {
	vk_proc_enqueue_blocked(vk_get_proc(socket_ptr->block.blocked_vk), socket_ptr);
}
int vk_socket_get_enqueued_blocked(struct vk_socket *socket_ptr) {
	return socket_ptr->blocked_enq;
}
void vk_socket_set_enqueued_blocked(struct vk_socket *socket_ptr, int blocked_enq) {
	socket_ptr->blocked_enq = blocked_enq;
}

/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket) {
	int rc;
	switch (socket->block.op) {
		case VK_OP_FLUSH:
			rc = vk_socket_handle_write(socket);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_WRITE:
			rc = vk_socket_handle_write(socket);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_READ:
			rc = vk_socket_handle_read(socket);
			if (rc == -1) {
				return -1;
			}
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	if (socket->block.blocked && socket->block.blocked_fd != -1) {
		vk_socket_enqueue_blocked(socket);
	}

	return rc;
}

void vk_block_init(struct vk_block *block, char *buf, size_t len, int op)  {
	block->buf = buf;
	block->len = len;
	block->op  = op;
	block->copied = 0;
}
ssize_t vk_block_commit(struct vk_block *block, ssize_t rc) {
	block->rc = rc;
	block->len -= rc;
	block->buf += rc;
	block->copied += rc;
	return rc;
}

size_t vk_block_get_committed(struct vk_block *block) {
	return block->copied;
}
