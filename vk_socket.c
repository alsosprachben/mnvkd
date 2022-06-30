#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "debug.h"

/* satisfy VK_OP_READ */
ssize_t vk_socket_handle_read(struct vk_socket *socket) {
	switch (socket->rx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_vectoring_read(&socket->rx.ring, vk_pipe_get_fd(&socket->rx_fd));
			if (vk_vectoring_has_effect(&socket->rx.ring)) {
				vk_vectoring_clear_effect(&socket->rx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
			}
			socket->block.blocked = vk_vectoring_rx_is_blocked(&socket->rx.ring);
			socket->block.blocked_fd = vk_pipe_get_fd(&socket->rx_fd);
			break;
		case VK_PIPE_VK_RX:
			vk_vectoring_splice(&socket->rx.ring, vk_pipe_get_rx(&socket->rx_fd));
			if (vk_vectoring_has_effect(&socket->rx.ring)) {
				vk_vectoring_clear_effect(&socket->rx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(vk_pipe_get_socket(&socket->rx_fd)->block.blocked_vk);
				vk_enqueue_run(vk_pipe_get_socket(&socket->rx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_rx_is_blocked(&socket->rx.ring);
			socket->block.blocked_fd = vk_pipe_get_fd(&socket->rx_fd);
			break;
		case VK_PIPE_VK_TX:
			vk_vectoring_splice(&socket->rx.ring, vk_pipe_get_tx(&socket->rx_fd));
			if (vk_vectoring_has_effect(&socket->rx.ring)) {
				vk_vectoring_clear_effect(&socket->rx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(vk_pipe_get_socket(&socket->rx_fd)->block.blocked_vk);
				vk_enqueue_run(vk_pipe_get_socket(&socket->rx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_rx_is_blocked(&socket->rx.ring);
			socket->block.blocked_fd = vk_pipe_get_fd(&socket->rx_fd);
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
			vk_vectoring_write(&socket->tx.ring, vk_pipe_get_fd(&socket->tx_fd));
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = vk_pipe_get_fd(&socket->tx_fd);
			break;
		case VK_PIPE_VK_RX:
			vk_vectoring_splice(vk_pipe_get_rx(&socket->tx_fd), &socket->tx.ring);
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(vk_pipe_get_socket(&socket->tx_fd)->block.blocked_vk);
				vk_enqueue_run(vk_pipe_get_socket(&socket->tx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = vk_pipe_get_fd(&socket->tx_fd);
			break;
		case VK_PIPE_VK_TX:
			vk_vectoring_splice(vk_pipe_get_tx(&socket->tx_fd), &socket->tx.ring);
			if (vk_vectoring_has_effect(&socket->tx.ring)) {
				vk_vectoring_clear_effect(&socket->tx.ring);
				/* self made progress, so continue self */
				/* vk_enqueue(socket->block.blocked_vk, socket->block.blocked_vk); */
				vk_ready(socket->block.blocked_vk);
				/* target made progress, so continue target */
				vk_ready(vk_pipe_get_socket(&socket->tx_fd)->block.blocked_vk);
				vk_enqueue_run(vk_pipe_get_socket(&socket->tx_fd)->block.blocked_vk); 
			}
			socket->block.blocked = vk_vectoring_tx_is_blocked(&socket->tx.ring);
			socket->block.blocked_fd = vk_pipe_get_fd(&socket->tx_fd);
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

int vk_socket_handle_tx_close(struct vk_socket *socket) {
	switch (socket->tx_fd.type) {
		case VK_PIPE_OS_FD:
			DBG("Closing FD %i\n", vk_pipe_get_fd(&socket->tx_fd));
			vk_vectoring_close(&socket->tx.ring, vk_pipe_get_fd(&socket->tx_fd));
			vk_ready(socket->block.blocked_vk);
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

int vk_socket_handle_rx_close(struct vk_socket *socket) {
	switch (socket->rx_fd.type) {
		case VK_PIPE_OS_FD:
			DBG("Closing FD %i\n", vk_pipe_get_fd(&socket->rx_fd));
			vk_vectoring_close(&socket->rx.ring, vk_pipe_get_fd(&socket->rx_fd));
			vk_ready(socket->block.blocked_vk);
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket->rx.ring.error != 0) {
		socket->error = socket->rx.ring.error;
	}

	return 0;
}

void vk_socket_init(struct vk_socket *socket_ptr, struct vk_thread *that, struct vk_pipe *rx_ptr, struct vk_pipe *tx_ptr) {
    VK_SOCKET_INIT(*socket_ptr, that, *rx_ptr, *tx_ptr);
}

size_t vk_socket_size(struct vk_socket *socket_ptr) {
	return sizeof (*socket_ptr);
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
		case VK_OP_TX_CLOSE:
			rc = vk_socket_handle_tx_close(socket);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_RX_CLOSE:
			rc = vk_socket_handle_rx_close(socket);
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

struct vk_pipe *vk_socket_get_rx_fd(struct vk_socket *socket_ptr) {
	return &socket_ptr->rx_fd;
}

struct vk_pipe *vk_socket_get_tx_fd(struct vk_socket *socket_ptr) {
	return &socket_ptr->tx_fd;
}

struct vk_vectoring *vk_socket_get_rx_vectoring(struct vk_socket *socket_ptr) {
	return &socket_ptr->rx.ring;
}

struct vk_vectoring *vk_socket_get_tx_vectoring(struct vk_socket *socket_ptr) {
	return &socket_ptr->tx.ring;
}

struct vk_block *vk_socket_get_block(struct vk_socket *socket_ptr) {
	return &socket_ptr->block;
}

struct vk_socket *vk_socket_next_blocked_socket(struct vk_socket *socket_ptr) {
	return SLIST_NEXT(socket_ptr, blocked_q_elem);
}

void vk_block_init(struct vk_block *block_ptr, char *buf, size_t len, int op)  {
	block_ptr->buf = buf;
	block_ptr->len = len;
	block_ptr->op  = op;
	block_ptr->copied = 0;
}
ssize_t vk_block_commit(struct vk_block *block_ptr, ssize_t rc) {
	block_ptr->rc = rc;
	block_ptr->len -= rc;
	block_ptr->buf += rc;
	block_ptr->copied += rc;
	return rc;
}

int vk_block_get_op(struct vk_block *block_ptr) {
	return block_ptr->op;
}

size_t vk_block_get_committed(struct vk_block *block_ptr) {
	return block_ptr->copied;
}

size_t vk_block_get_uncommitted(struct vk_block *block_ptr) {
	return block_ptr->len;
}

void vk_block_set_uncommitted(struct vk_block *block_ptr, size_t len) {
	block_ptr->len = len;
}

char *vk_block_get_buf(struct vk_block *block_ptr) {
	return block_ptr->buf;
}

struct vk_thread *vk_block_get_vk(struct vk_block *block_ptr) {
	return block_ptr->blocked_vk;
}

void vk_block_set_vk(struct vk_block *block_ptr, struct vk_thread *blocked_vk) {
	block_ptr->blocked_vk = blocked_vk;
}