#include "vk_socket.h"
#include "debug.h"

void vk_enqueue(struct that *that, struct that *there);
void vk_ready(struct that *that);

/* satisfy VK_OP_READ */
ssize_t vk_socket_read(struct vk_socket *socket) {
	switch (socket->rx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_vectoring_read(&socket->rx.ring, VK_PIPE_GET_FD(socket->rx_fd));
			if (vk_vectoring_has_effect(&socket->rx.ring)) {
				vk_vectoring_clear_effect(&socket->rx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				socket->block.effect = 1;
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
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk); 
				socket->block.effect = 1;
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
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk); 
				socket->block.effect = 1;
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
ssize_t vk_socket_write(struct vk_socket *socket) {
	switch (socket->tx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_vectoring_write(&socket->tx.ring, VK_PIPE_GET_FD(socket->tx_fd));
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
				socket->block.effect = 1;
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->tx_fd);
			break;
		case VK_PIPE_VK_RX:
			vk_vectoring_splice(VK_PIPE_GET_RX(socket->tx_fd), &socket->tx.ring);
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk); 
				socket->block.effect = 1;
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = VK_PIPE_GET_FD(socket->tx_fd);
			break;
		case VK_PIPE_VK_TX:
			vk_vectoring_splice(VK_PIPE_GET_TX(socket->tx_fd), &socket->tx.ring);
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk); 
				socket->block.effect = 1;
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

/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket) {
	int rc;
	switch (socket->block.op) {
		case VK_OP_FLUSH:
			rc = vk_socket_write(socket);
			if (rc == -1) {
				return -1;
			}
			return socket->block.effect;
		case VK_OP_WRITE:
			rc = vk_socket_write(socket);
			if (rc == -1) {
				return -1;
			}
			return socket->block.effect;
		case VK_OP_READ:
			rc = vk_socket_read(socket);
			if (rc == -1) {
				return -1;
			}
			return socket->block.effect;
	}

	return 0;
}

