#include "vk_socket.h"
#include "debug.h"

void vk_enqueue(struct that *that, struct that *there);

/* satisfy VK_OP_READ */
ssize_t vk_socket_read(struct vk_socket *socket) {
	ssize_t received;
	switch (socket->rx_fd.type) {
		case VK_PIPE_OS_FD:
			received = vk_vectoring_read(&socket->rx.ring, VK_PIPE_GET_FD(socket->rx_fd));
			if (received > 0) {
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
			}
			break;
		case VK_PIPE_VK_RX:
			received = vk_vectoring_splice(&socket->rx.ring, VK_PIPE_GET_RX(socket->rx_fd));
			if (received > 0) {
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk); 
			}
			break;
		case VK_PIPE_VK_TX:
			received = vk_vectoring_splice(&socket->rx.ring, VK_PIPE_GET_TX(socket->rx_fd));
			if (received > 0) {
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->rx_fd)->block.blocked_vk); 
			}
			break;
		default:
			errno = EINVAL;
			received = -1;
			break;
	}
	if (received == -1) {
		socket->error = socket->rx.ring.error;
	}
	return received;
}

/* satisfy VK_OP_WRITE */
ssize_t vk_socket_write(struct vk_socket *socket) {
	ssize_t sent;
	switch (socket->tx_fd.type) {
		case VK_PIPE_OS_FD:
			sent = vk_vectoring_write(&socket->tx.ring, VK_PIPE_GET_FD(socket->tx_fd));
			if (sent > 0) {
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
			}
			break;
		case VK_PIPE_VK_RX:
			sent = vk_vectoring_splice(VK_PIPE_GET_RX(socket->tx_fd), &socket->tx.ring);
			if (sent > 0) {
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk); 
			}
			break;
		case VK_PIPE_VK_TX:
			sent = vk_vectoring_splice(VK_PIPE_GET_TX(socket->tx_fd), &socket->tx.ring);
			if (sent > 0) {
				/* self made progress, so continue self */
				vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_enqueue(socket->block.blocked_vk, VK_PIPE_GET_SOCKET(socket->tx_fd)->block.blocked_vk); 
			}
			if (sent == -1 && errno == EAGAIN) {
				sent = 0;
				errno = 0;
			}
			break;
		default:
			errno = EINVAL;
			sent = -1;
			break;
	}
	if (sent == -1) {
		socket->error = socket->tx.ring.error;
	}
	return sent;
}

/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket) {
	ssize_t sent;
	ssize_t received;
	switch (socket->block.op) {
		case VK_OP_WRITE:
			sent = vk_socket_write(socket);
			if (sent == -1) {
				return -1;
			}
			break;
		case VK_OP_READ:
			received = vk_socket_read(socket);
			if (received == -1) {
				return -1;
			}
			break;
	}

	return 0;
}

