#include "vk_socket.h"

/* satisfy VK_OP_READ */
ssize_t vk_socket_read(struct vk_socket *socket) {
	ssize_t received;
	received = vk_vectoring_read(&socket->rx.ring, socket->rx_fd);
	if (received == -1) {
		socket->error = socket->rx.ring.error;
	}
	return received;
}

/* satisfy VK_OP_WRITE */
ssize_t vk_socket_write(struct vk_socket *socket) {
	ssize_t sent;
	sent = vk_vectoring_write(&socket->tx.ring, socket->tx_fd);
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

