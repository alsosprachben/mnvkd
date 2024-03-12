/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "vk_vectoring.h"
#include "vk_fd.h"
#include "vk_io_future.h"
#include "vk_debug.h"


/* whether vector effect needs to be handled */
int vk_socket_handle_rx_effect(struct vk_socket *socket_ptr) {
    return vk_vectoring_handle_effect(&socket_ptr->rx.ring);
}
int vk_socket_handle_tx_effect(struct vk_socket *socket_ptr) {
    return vk_vectoring_handle_effect(&socket_ptr->tx.ring);
}

/* handle vector block */
void vk_socket_handle_rx_block(struct vk_socket *socket_ptr) {
    socket_ptr->block.blocked = vk_vectoring_rx_is_blocked(&socket_ptr->rx.ring);
    socket_ptr->block.blocked_events = POLLIN;
    socket_ptr->block.blocked_fd = vk_pipe_get_fd(&socket_ptr->rx_fd);
}
void vk_socket_handle_tx_block(struct vk_socket *socket_ptr) {
    socket_ptr->block.blocked = vk_vectoring_tx_is_blocked(&socket_ptr->tx.ring);
    socket_ptr->block.blocked_events = POLLOUT;
    socket_ptr->block.blocked_fd = vk_pipe_get_fd(&socket_ptr->tx_fd);
}
int vk_socket_is_blocked(struct vk_socket *socket_ptr) {
    return socket_ptr->block.blocked;
}

void vk_socket_signed_received(struct vk_socket *socket_ptr, ssize_t rc) {
    if (vk_socket_handle_rx_effect(socket_ptr)) {
        /* drain from available */
        vk_socket_drain_readable(socket_ptr, rc);
        /* self made progress, so continue self */
        /* vk_enqueue_run(socket_ptr->block.blocked_vk); */
        vk_ready(socket_ptr->block.blocked_vk);
    }
}
void vk_socket_signed_sent(struct vk_socket *socket_ptr, ssize_t rc) {
    if (vk_socket_handle_tx_effect(socket_ptr)) {
        /* drain from available */
        vk_socket_drain_writable(socket_ptr, rc);
        /* self made progress, so continue self */
        /* vk_enqueue_run(socket_ptr->block.blocked_vk); */
        vk_ready(socket_ptr->block.blocked_vk);
    }
}

/* satisfy VK_OP_READ */
ssize_t vk_socket_handle_read(struct vk_socket *socket_ptr) {
	ssize_t rc;
	switch (socket_ptr->rx_fd.type) {
		case VK_PIPE_OS_FD:
			rc = vk_vectoring_read(&socket_ptr->rx.ring, vk_pipe_get_fd(&socket_ptr->rx_fd));
			vk_socket_signed_received(socket_ptr, rc);
			vk_socket_handle_rx_block(socket_ptr);
			break;
		case VK_PIPE_VK_TX:
			rc = vk_vectoring_splice(&socket_ptr->rx.ring, vk_pipe_get_tx(&socket_ptr->rx_fd), -1);
			vk_socket_signed_received(socket_ptr, rc);
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket_ptr->tx.ring.error != 0) {
		socket_ptr->error = socket_ptr->rx.ring.error;
	}
	return 0;
}

/* satisfy VK_OP_WRITE */
ssize_t vk_socket_handle_write(struct vk_socket *socket_ptr) {
	ssize_t rc;
	switch (socket_ptr->tx_fd.type) {
		case VK_PIPE_OS_FD:
			rc = vk_vectoring_write(&socket_ptr->tx.ring, vk_pipe_get_fd(&socket_ptr->tx_fd));
			vk_socket_signed_sent(socket_ptr, rc);
			vk_socket_handle_tx_block(socket_ptr);
			break;
		case VK_PIPE_VK_RX:
			rc = vk_vectoring_splice(vk_pipe_get_rx(&socket_ptr->tx_fd), &socket_ptr->tx.ring, -1);
			vk_socket_signed_sent(socket_ptr, rc);
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket_ptr->tx.ring.error != 0) {
		socket_ptr->error = socket_ptr->tx.ring.error;
	}
	return 0;
}

/* satisfy VK_OP_SPLICE */
ssize_t vk_socket_handle_splice(struct vk_socket *socket_ptr) {
	ssize_t rc;
    /* since writes buffer directly into read buffers, writes can be blocked by blocked reads, so if both are blocked, register the read block */
	switch (socket_ptr->rx_fd.type) {
		case VK_PIPE_OS_FD:
		    /* read from OS file descriptor */
            rc = vk_vectoring_read(&socket_ptr->rx.ring, vk_pipe_get_fd(&socket_ptr->rx_fd));
            vk_socket_signed_received(socket_ptr, rc);
            vk_socket_handle_rx_block(socket_ptr);

            switch (socket_ptr->tx_fd.type) {
                case VK_PIPE_OS_FD:
                    /* write to OS file descriptor */
                    rc = vk_vectoring_write(&socket_ptr->tx.ring, vk_pipe_get_fd(&socket_ptr->tx_fd));
                    vk_socket_signed_sent(socket_ptr, rc);
                    if ( ! vk_socket_is_blocked(socket_ptr)) {
                        vk_socket_handle_tx_block(socket_ptr);
                    }
                    break;
                case VK_PIPE_VK_RX:
                    /* write to receive side of virtual socket */
                    rc = vk_vectoring_splice(vk_pipe_get_rx(&socket_ptr->tx_fd), &socket_ptr->tx.ring, -1);
                    vk_socket_signed_sent(socket_ptr, rc);
                    break;
                default:
                    errno = EINVAL;
                    return -1;
                    break;
            }
			break;
		case VK_PIPE_VK_RX:
            /* read from receive side of virtual socket */
            rc = vk_vectoring_splice(&socket_ptr->rx.ring, vk_pipe_get_tx(&socket_ptr->rx_fd), -1);
            vk_socket_signed_received(socket_ptr, rc);
		    switch (socket_ptr->tx_fd.type) {
		        case VK_PIPE_OS_FD:
		            /* write to OS file descriptor */
                    rc = vk_vectoring_write(&socket_ptr->tx.ring, vk_pipe_get_fd(&socket_ptr->tx_fd));
                    vk_socket_signed_sent(socket_ptr, rc);
                    if ( ! vk_socket_is_blocked(socket_ptr)) {
                        vk_socket_handle_tx_block(socket_ptr);
                    }
                    break;
                case VK_PIPE_VK_RX:
                    /* write to receive side of virtual socket */
                    rc = vk_vectoring_splice(vk_pipe_get_rx(&socket_ptr->tx_fd), &socket_ptr->tx.ring, -1);
                    vk_socket_signed_sent(socket_ptr, rc);
                    break;
                default:
                    errno = EINVAL;
                    return -1;
                    break;
		    }
		    break;
        default:
            errno = EINVAL;
            return -1;
            break;
    }
	if (socket_ptr->tx.ring.error != 0) {
		socket_ptr->error = socket_ptr->rx.ring.error;
	}
	return 0;
}

/* satisfy VK_OP_HUP */
ssize_t vk_socket_handle_hup(struct vk_socket *socket_ptr) {
	switch (socket_ptr->tx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_socket_signed_sent(socket_ptr, 0);
			vk_socket_handle_tx_block(socket_ptr);
			break;
		case VK_PIPE_VK_RX:
			vk_vectoring_mark_eof(vk_pipe_get_rx(&socket_ptr->tx_fd));
			vk_vectoring_clear_eof(&socket_ptr->tx.ring);
			vk_socket_signed_sent(socket_ptr, 0);
			break;
		case VK_PIPE_VK_TX:
		    errno = EINVAL;
		    return -1;
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket_ptr->tx.ring.error != 0) {
		socket_ptr->error = socket_ptr->tx.ring.error;
	}
	return 0;
}

int vk_socket_handle_tx_close(struct vk_socket *socket_ptr) {
	switch (socket_ptr->tx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_socket_dbgf("closing write-side FD %i\n", vk_pipe_get_fd(&socket_ptr->tx_fd));
			vk_vectoring_close(&socket_ptr->tx.ring, vk_pipe_get_fd(&socket_ptr->tx_fd));
			vk_ready(socket_ptr->block.blocked_vk);
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket_ptr->tx.ring.error != 0) {
		socket_ptr->error = socket_ptr->tx.ring.error;
	}

	vk_pipe_set_closed(&socket_ptr->tx_fd, 1);

	return 0;
}

int vk_socket_handle_rx_close(struct vk_socket *socket_ptr) {
	switch (socket_ptr->rx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_socket_dbgf("closing read-side FD %i\n", vk_pipe_get_fd(&socket_ptr->rx_fd));
			vk_vectoring_close(&socket_ptr->rx.ring, vk_pipe_get_fd(&socket_ptr->rx_fd));
			vk_ready(socket_ptr->block.blocked_vk);
			break;
		default:
			errno = EINVAL;
			return -1;
			break;
	}
	if (socket_ptr->rx.ring.error != 0) {
		socket_ptr->error = socket_ptr->rx.ring.error;
	}

	vk_pipe_set_closed(&socket_ptr->rx_fd, 1);

	return 0;
}

/* explicit poll for readable */
int vk_socket_handle_readable(struct vk_socket *socket_ptr) {
	socket_ptr->block.blocked = 1;
	socket_ptr->block.blocked_events = POLLIN;
	socket_ptr->block.blocked_fd = vk_pipe_get_fd(&socket_ptr->rx_fd);
	return 0;
}

/* explicit poll for writable */
int vk_socket_handle_writable(struct vk_socket *socket_ptr) {
	socket_ptr->block.blocked = 1;
	socket_ptr->block.blocked_events = POLLOUT;
	socket_ptr->block.blocked_fd = vk_pipe_get_fd(&socket_ptr->tx_fd);
	return 0;
}

void vk_socket_init(struct vk_socket *socket_ptr, struct vk_thread *that, struct vk_pipe *rx_ptr, struct vk_pipe *tx_ptr) {
    VK_SOCKET_INIT(*socket_ptr, that, *rx_ptr, *tx_ptr);
}

size_t vk_socket_size(struct vk_socket *socket_ptr) {
	return sizeof (*socket_ptr);
}

void vk_socket_enqueue_blocked(struct vk_socket *socket_ptr) {
	vk_proc_local_enqueue_blocked(vk_get_proc_local(socket_ptr->block.blocked_vk), socket_ptr);
}
int vk_socket_get_enqueued_blocked(struct vk_socket *socket_ptr) {
	return socket_ptr->blocked_enq;
}
void vk_socket_set_enqueued_blocked(struct vk_socket *socket_ptr, int blocked_enq) {
	socket_ptr->blocked_enq = blocked_enq;
}

/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket_ptr) {
	int rc;

	vk_socket_dbg("handling I/O");

	switch (socket_ptr->block.op) {
		case VK_OP_NONE:
			socket_ptr->block.blocked = 0;
			socket_ptr->block.blocked_events = 0;
			socket_ptr->block.blocked_fd = -1;
			rc = 0;
			break;
		case VK_OP_FLUSH:
			rc = vk_socket_handle_write(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_WRITE:
			rc = vk_socket_handle_write(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_READ:
			rc = vk_socket_handle_read(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_SPLICE:
		    rc = vk_socket_handle_splice(socket_ptr);
		    if (rc == -1) {
		        return -1;
            }
		case VK_OP_HUP:
			rc = vk_socket_handle_hup(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_TX_CLOSE:
			rc = vk_socket_handle_tx_close(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_RX_CLOSE:
			rc = vk_socket_handle_rx_close(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_READABLE:
			rc = vk_socket_handle_readable(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		case VK_OP_WRITABLE:
			rc = vk_socket_handle_writable(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	if (socket_ptr->error != 0) {
		vk_socket_perror("after I/O handling");
	}

	if (socket_ptr->block.blocked && socket_ptr->block.blocked_fd != -1) {
		vk_socket_enqueue_blocked(socket_ptr);
	}

	vk_socket_dbg("handled I/O");

	return rc;
}

int vk_socket_get_error(struct vk_socket *socket_ptr) {
	return socket_ptr->error;
}
void vk_socket_set_error(struct vk_socket *socket_ptr, int error) {
	socket_ptr->error = error;
}

size_t vk_socket_get_bytes_readable(struct vk_socket *socket_ptr) {
	return socket_ptr->bytes_readable;
}
void vk_socket_set_bytes_readable(struct vk_socket *socket_ptr, size_t bytes_readable) {
	socket_ptr->bytes_readable = bytes_readable;
}

size_t vk_socket_get_bytes_writable(struct vk_socket *socket_ptr) {
	return socket_ptr->bytes_writable;
}
void vk_socket_set_bytes_writable(struct vk_socket *socket_ptr, size_t bytes_writable) {
	socket_ptr->bytes_writable = bytes_writable;
}

int vk_socket_get_not_readable(struct vk_socket *socket_ptr) {
	return socket_ptr->bytes_readable;
}
void vk_socket_set_not_readable(struct vk_socket *socket_ptr, int not_readable) {
	socket_ptr->not_readable = not_readable;
}

int vk_socket_get_not_writable(struct vk_socket *socket_ptr) {
	return socket_ptr->not_writable;
}
void vk_socket_set_not_writable(struct vk_socket *socket_ptr, int not_writable) {
	socket_ptr->not_writable = not_writable;
}

void vk_socket_drain_readable(struct vk_socket *socket_ptr, size_t len) {
	if (len >= socket_ptr->bytes_readable) {
		socket_ptr->bytes_readable = 0;
	} else {
		socket_ptr->bytes_readable -= len;
	}
}
void vk_socket_drain_writable(struct vk_socket *socket_ptr, size_t len) {
	if (len > socket_ptr->bytes_writable) {
		socket_ptr->bytes_writable = 0;
	} else {
		socket_ptr->bytes_writable -= len;
	}
}

void vk_socket_mark_not_readable(struct vk_socket *socket_ptr) {
	socket_ptr->not_readable = 1;
}
void vk_socket_mark_not_writable(struct vk_socket *socket_ptr) {
	socket_ptr->not_writable = 1;
}

void vk_socket_apply_fd(struct vk_socket *socket_ptr, struct vk_fd *fd_ptr) {
	struct vk_io_future *ioft_ptr;
	ioft_ptr = vk_fd_get_ioft_ret(fd_ptr);
	socket_ptr->bytes_readable = vk_io_future_get_readable(ioft_ptr);
	socket_ptr->bytes_writable = vk_io_future_get_writable(ioft_ptr);
	socket_ptr->not_readable = socket_ptr->bytes_readable == 0;
	socket_ptr->not_writable = socket_ptr->bytes_writable == 0;
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

int vk_socket_get_blocked_fd(struct vk_socket *socket_ptr) {
    return vk_block_get_fd(&socket_ptr->block);
}
int vk_socket_get_blocked_events(struct vk_socket *socket_ptr) {
    return vk_block_get_events(&socket_ptr->block);
}
int vk_socket_get_blocked_closed(struct vk_socket *socket_ptr) {
	int closed = 0;
	switch (vk_block_get_op(vk_socket_get_block(socket_ptr))) {
		case VK_OP_READ:
		case VK_OP_READABLE:
			closed = vk_pipe_get_closed(vk_socket_get_rx_fd(socket_ptr));
			break;
		case VK_OP_WRITE:
		case VK_OP_FLUSH:
		case VK_OP_WRITABLE:
		case VK_OP_HUP:
			closed = vk_pipe_get_closed(vk_socket_get_tx_fd(socket_ptr));
			break;
		case VK_OP_SPLICE:
		    closed = vk_pipe_get_closed(vk_socket_get_rx_fd(socket_ptr))
		          || vk_pipe_get_closed(vk_socket_get_tx_fd(socket_ptr));
		    break;
	}
	return closed;
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

const char *vk_block_get_op_str(struct vk_block *block_ptr) {
	switch (block_ptr->op) {
		case VK_OP_NONE: return "none";
		case VK_OP_READ: return "read";
		case VK_OP_WRITE: return "write";
		case VK_OP_FLUSH: return "flush";
		case VK_OP_SPLICE: return "splice";
		case VK_OP_HUP: return "hup";
		case VK_OP_TX_CLOSE: return "tx_close";
		case VK_OP_RX_CLOSE: return "rx_close";
		case VK_OP_READABLE: return "readable";
		case VK_OP_WRITABLE: return "writable";
	}
	return "";
}

void vk_block_set_op(struct vk_block *block_ptr, int op) {
	block_ptr->op = op;
}

int vk_block_get_blocked(struct vk_block *block_ptr) {
	return block_ptr->blocked;
}

void vk_block_set_blocked(struct vk_block *block_ptr, int blocked) {
	block_ptr->blocked = blocked;
}

int vk_block_get_events(struct vk_block *block_ptr) {
    return block_ptr->blocked_events;
}

void vk_block_set_events(struct vk_block *block_ptr, int blocked_events) {
    block_ptr->blocked_events = blocked_events;
}

int vk_block_get_fd(struct vk_block *block_ptr) {
	return block_ptr->blocked_fd;
}

void vk_block_set_fd(struct vk_block *block_ptr, int blocked_fd){
	block_ptr->blocked_fd = blocked_fd;
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

void vk_block_set_buf(struct vk_block *block_ptr, char *buf) {
	block_ptr->buf = buf;
}

size_t vk_block_get_len(struct vk_block *block_ptr) {
	return block_ptr->len;
}

void vk_block_set_len(struct vk_block *block_ptr, size_t len) {
	block_ptr->len = len;
}

struct vk_thread *vk_block_get_vk(struct vk_block *block_ptr) {
	return block_ptr->blocked_vk;
}

void vk_block_set_vk(struct vk_block *block_ptr, struct vk_thread *blocked_vk) {
	block_ptr->blocked_vk = blocked_vk;
}
