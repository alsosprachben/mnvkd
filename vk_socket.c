/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "vk_vectoring.h"
#include "vk_fd.h"
#include "vk_io_future.h"
#include "vk_debug.h"

/*
 * == Blocking State Hierarchy ==
 * - [vk_proc_local vk_socket] vk_vectoring::[rt]x_blocked
 * - [vk_proc_local vk_socket] vk_socket::block.ioft_[rt]x_{pre,ret}.event [pollfd]
 * - [vk_fd_table vk_fd] vk_fd::ioft_{pre,post,ret}.event [pollfd]
 *
 * == I/O State Hierarchy ==
 * - [vk_proc_local vk_socket] vk_socket
 * - [vk_fd_table vk_fd] vk_fd::socket
 * - [vk_fd_table vk_fd] vk_fd::fd
 *
 * == Receive State Lifecycle ==
 * - vk_vectoring::rx_blocked = 1
 * - block::ioft_rx_pre.event & POLLIN
 * - _Enter Kernel_
 * - _Exit Local Process_
 * - vk_fd::ioft_pre.event & POLLIN
 * - Register with poller
 * - Returned from poller
 * - read() from vk_fd::fd into vk_fd::socket.rx
 * - vk_fd::ioft_post.r?event & POLLIN
 * - vk_fd::ioft_ret.r?event & POLLIN
 * - _Enter Local Process_
 * - read from vk_fd::socket.rx to vk_socket.rx
 * - _Exit Kernel_
 * - block::ioft_rx_ret.r?event & POLLIN
 * - vk_vectoring::rx_blocked = 0
 *
 * == Send State Lifecycle ==
 * - vk_vectoring::tx_blocked = 1
 * - block::ioft_tx_pre.event & POLLOUT
 * - _Enter Kernel_
 * - _Exit Local Process_
 * - vk_fd::ioft_pre.event & POLLOUT
 * - Register with poller
 * - Returned from poller
 * - write() vk_fd::socket.tx into vk_fd::fd
 * - vk_fd::ioft_post.r?event & POLLOUT
 * - vk_fd::ioft_ret.r?event & POLLOUT
 * - _Enter Local Process_
 * - write to vk_fd::socket.tx from vk_socket.tx
 * - _Exit Kernel_
 * - block::ioft_tx_ret.r?event & POLLOUT
 * - vk_vectoring::tx_blocked = 0
 */

struct vk_pipe *vk_socket_get_reader_pipe(struct vk_socket *socket_ptr) {
    return &socket_ptr->rx_fd;
}
struct vk_pipe *vk_socket_get_writer_pipe(struct vk_socket *socket_ptr) {
    return &socket_ptr->tx_fd;
}

/* reader's socket, or NULL if FD */
struct vk_socket *vk_socket_get_reader_socket(struct vk_socket *socket_ptr) {
    return vk_pipe_get_socket(vk_socket_get_reader_pipe(socket_ptr));
}
/* writer's socket, or NULL if FD */
struct vk_socket *vk_socket_get_writer_socket(struct vk_socket *socket_ptr) {
    return vk_pipe_get_socket(vk_socket_get_writer_pipe(socket_ptr));
}

/* reader's FD, or -1 if socket */
int vk_socket_get_reader_fd(struct vk_socket *socket_ptr) {
    return vk_pipe_get_fd(vk_socket_get_reader_pipe(socket_ptr));
}
/* writer's FD, or -1 if socket */
int vk_socket_get_writer_fd(struct vk_socket *socket_ptr) {
    return vk_pipe_get_fd(vk_socket_get_writer_pipe(socket_ptr));
}

struct vk_vectoring *vk_socket_get_read_ring(struct vk_socket *socket_ptr) {
    return &socket_ptr->rx.ring;
}
struct vk_vectoring *vk_socket_get_write_ring(struct vk_socket *socket_ptr) {
    return &socket_ptr->tx.ring;
}

void vk_socket_handle_error(struct vk_socket *socket_ptr) {
    socket_ptr->error = errno;
}

/* whether vector effect needs to be handled */
int vk_socket_handle_rx_effect(struct vk_socket *socket_ptr) {
    return vk_vectoring_handle_effect(&socket_ptr->rx.ring);
}
int vk_socket_handle_tx_effect(struct vk_socket *socket_ptr) {
    return vk_vectoring_handle_effect(&socket_ptr->tx.ring);
}

void vk_socket_mark_read_block(struct vk_socket *socket_ptr) {
    socket_ptr->block.ioft_rx_pre.event.events = POLLIN;
}
void vk_socket_clear_read_block(struct vk_socket *socket_ptr) {
    socket_ptr->block.ioft_rx_pre.event.events = 0;
}
void vk_socket_mark_write_block(struct vk_socket *socket_ptr) {
    socket_ptr->block.ioft_tx_pre.event.events = POLLOUT;
}
void vk_socket_clear_write_block(struct vk_socket *socket_ptr) {
    socket_ptr->block.ioft_tx_pre.event.events = 0;
}

/* make socket IO futures from blocked FDs on the socket */
void vk_socket_handle_block(struct vk_socket *socket_ptr) {
    vk_block_get_ioft_rx_pre(vk_socket_get_block(socket_ptr))->event.fd = vk_pipe_get_fd(&socket_ptr->rx_fd);
    vk_block_get_ioft_tx_pre(vk_socket_get_block(socket_ptr))->event.fd = vk_pipe_get_fd(&socket_ptr->tx_fd);


    /* Forward closed state from rings to pipes. */
    /* for read */
    vk_pipe_set_closed(&socket_ptr->rx_fd, vk_vectoring_is_closed(&socket_ptr->rx.ring));
    if (vk_pipe_get_fd(&socket_ptr->rx_fd) >= 0 && vk_pipe_get_closed(&socket_ptr->rx_fd)) {
        /* Forward closed state from pipe to IO future. */
        vk_io_future_set_rx_closed(vk_block_get_ioft_rx_pre(vk_socket_get_block(socket_ptr)), vk_pipe_get_closed(&socket_ptr->rx_fd));
    }
    /* for write */
    vk_pipe_set_closed(&socket_ptr->tx_fd, vk_vectoring_is_closed(&socket_ptr->tx.ring));
    if (vk_pipe_get_fd(&socket_ptr->tx_fd) >= 0 && vk_pipe_get_closed(&socket_ptr->tx_fd)) {
        /* Forward closed state from pipe to IO future. */
        vk_io_future_set_tx_closed(vk_block_get_ioft_tx_pre(vk_socket_get_block(socket_ptr)), vk_pipe_get_closed(&socket_ptr->tx_fd));
    }

    /* Build IO future and register poll. */
    /* for read */
    if (vk_pipe_get_fd(&socket_ptr->rx_fd) >= 0) {
        if (vk_vectoring_rx_is_blocked(&socket_ptr->rx.ring)) {
            /* Set IO future to poll for read. */
            vk_socket_mark_read_block(socket_ptr);
            /* register poll */
            vk_socket_enqueue_blocked(socket_ptr);
        } else {
            vk_socket_clear_read_block(socket_ptr);
        }
    } else {
        vk_socket_clear_read_block(socket_ptr);
    }
    /* for write */
    if (vk_pipe_get_fd(&socket_ptr->tx_fd) >= 0) {
        if (vk_vectoring_tx_is_blocked(&socket_ptr->tx.ring)
        ) {
            /* Set IO future to poll for write. */
            vk_socket_mark_write_block(socket_ptr);
            /* register poll */
            vk_socket_enqueue_blocked(socket_ptr);
        } else {
            vk_socket_clear_write_block(socket_ptr);
        }
    } else {
        vk_socket_clear_write_block(socket_ptr);
    }
}

int vk_socket_handle_readable(struct vk_socket *socket_ptr) {
    vk_socket_mark_read_block(socket_ptr);
    vk_socket_clear_write_block(socket_ptr);
    return 0;
}
int vk_socket_handle_writable(struct vk_socket *socket_ptr) {
    vk_socket_mark_write_block(socket_ptr);
    vk_socket_clear_read_block(socket_ptr);
    return 0;
}

void vk_socket_enqueue_self(struct vk_socket *socket_ptr) {
    /* enqueue self */
    /* vk_enqueue_run(socket_ptr->block.blocked_vk); -- execution loop implies this */
    vk_ready(socket_ptr->block.blocked_vk);
}
void vk_socket_enqueue_otherwriter(struct vk_socket *socket_ptr) {
    /* enqueue writer of our reads */
    vk_enqueue_run(vk_pipe_get_socket(&socket_ptr->rx_fd)->block.blocked_vk);
    vk_ready(vk_pipe_get_socket(&socket_ptr->rx_fd)->block.blocked_vk);
}
void vk_socket_enqueue_otherreader(struct vk_socket *socket_ptr) {
    /* enqueue reader of our writes */
    vk_enqueue_run(vk_pipe_get_socket(&socket_ptr->tx_fd)->block.blocked_vk);
    vk_ready(vk_pipe_get_socket(&socket_ptr->tx_fd)->block.blocked_vk);
}

void vk_socket_enqueue_read(struct vk_socket *socket_ptr) {
    if (vk_socket_handle_rx_effect(socket_ptr)) {
        vk_socket_enqueue_self(socket_ptr);
    }
}
void vk_socket_enqueue_write(struct vk_socket *socket_ptr) {
    if (vk_socket_handle_tx_effect(socket_ptr)) {
        vk_socket_enqueue_self(socket_ptr);
    }
}

void vk_socket_enqueue_readwriter(struct vk_socket *socket_ptr) {
    if (vk_socket_handle_tx_effect(vk_pipe_get_socket(&socket_ptr->rx_fd))) {
        vk_socket_enqueue_otherwriter(socket_ptr);
        vk_socket_enqueue_self(socket_ptr);

    }
}

void vk_socket_enqueue_writereader(struct vk_socket *socket_ptr) {
    if (vk_socket_handle_rx_effect(vk_pipe_get_socket(&socket_ptr->tx_fd))) {
        vk_socket_enqueue_otherreader(socket_ptr);
        vk_socket_enqueue_self(socket_ptr);
    }
}

/* satisfy VK_OP_READ */
ssize_t vk_socket_handle_read(struct vk_socket *socket_ptr) {
	ssize_t rc = 0;
    vk_socket_dbg("read");
	switch (socket_ptr->rx_fd.type) {
		case VK_PIPE_OS_FD:
            switch (socket_ptr->rx_fd.fd_type) {
                case VK_FD_TYPE_SOCKET_LISTEN:
                    rc = vk_vectoring_accept(&socket_ptr->rx.ring, vk_pipe_get_fd(&socket_ptr->rx_fd));
                    if (rc == -1) {
                        vk_socket_handle_error(socket_ptr);
                    }
                    rc = 0;
                    break;
                default:
                    rc = vk_vectoring_read(&socket_ptr->rx.ring, vk_pipe_get_fd(&socket_ptr->rx_fd));
                    if (rc == -1) {
                        vk_socket_handle_error(socket_ptr);
                    }
                    rc = 0;
                    break;
            }
			vk_socket_enqueue_read(socket_ptr);
			break;
		case VK_PIPE_VK_TX:
			rc = vk_vectoring_splice(&socket_ptr->rx.ring, vk_pipe_get_tx(&socket_ptr->rx_fd), -1);
            if (rc == -1) {
                vk_socket_handle_error(socket_ptr);
            }
            rc = 0;
            vk_socket_enqueue_read(socket_ptr);
            vk_socket_enqueue_readwriter(socket_ptr);
			break;
		default:
			errno = EINVAL;
			rc = -1;
            break;
	}
	vk_socket_handle_block(socket_ptr);
	return rc;
}

/* satisfy VK_OP_WRITE */
ssize_t vk_socket_handle_write(struct vk_socket *socket_ptr) {
	ssize_t rc = 0;
    vk_socket_dbg("write");
	switch (socket_ptr->tx_fd.type) {
		case VK_PIPE_OS_FD:
			rc = vk_vectoring_write(&socket_ptr->tx.ring, vk_pipe_get_fd(&socket_ptr->tx_fd));
            if (rc == -1) {
                vk_socket_handle_error(socket_ptr);
            }
            rc = 0;
			vk_socket_enqueue_write(socket_ptr);
			break;
		case VK_PIPE_VK_RX:
			rc = vk_vectoring_splice(vk_pipe_get_rx(&socket_ptr->tx_fd), &socket_ptr->tx.ring, -1);
            if (rc == -1) {
                vk_socket_handle_error(socket_ptr);
            }
            rc = 0;
			vk_socket_enqueue_write(socket_ptr);
            vk_socket_enqueue_writereader(socket_ptr);
			break;
		default:
			errno = EINVAL;
			rc = -1;
            break;
	}
	vk_socket_handle_block(socket_ptr);
	return rc;
}

/* satisfy VK_OP_FORWARD */
ssize_t vk_socket_handle_forward(struct vk_socket *socket_ptr) {
	ssize_t rc = 0;
    vk_socket_dbg("forward");
    /* since writes buffer directly into read buffers, writes can be blocked by blocked reads, so if both are blocked, register the read block */
	switch (socket_ptr->rx_fd.type) {
		case VK_PIPE_OS_FD:
		    /* read from OS file descriptor */
            rc = vk_vectoring_read(&socket_ptr->rx.ring, vk_pipe_get_fd(&socket_ptr->rx_fd));
            if (rc == -1) {
                vk_socket_handle_error(socket_ptr);
            }
            rc = 0;
            vk_socket_enqueue_read(socket_ptr);

            switch (socket_ptr->tx_fd.type) {
                case VK_PIPE_OS_FD:
                    /* write to OS file descriptor */
                    rc = vk_vectoring_write(&socket_ptr->tx.ring, vk_pipe_get_fd(&socket_ptr->tx_fd));
                    if (rc == -1) {
                        vk_socket_handle_error(socket_ptr);
                    }
                    rc = 0;
                    vk_socket_enqueue_write(socket_ptr);
                    break;
                case VK_PIPE_VK_RX:
                    /* write to receive side of virtual socket */
                    rc = vk_vectoring_splice(vk_pipe_get_rx(&socket_ptr->tx_fd), &socket_ptr->tx.ring, -1);
                    if (rc == -1) {
                        vk_socket_handle_error(socket_ptr);
                    }
                    rc = 0;
                    vk_socket_enqueue_write(socket_ptr);
                    /* may have unblocked the other side */
                    if ( ! vk_vectoring_rx_is_blocked(vk_pipe_get_rx(&socket_ptr->tx_fd))) {
                        vk_socket_enqueue_writereader(socket_ptr);
                        vk_enqueue_run(vk_pipe_get_socket(&socket_ptr->tx_fd)->block.blocked_vk);
                    }
                    break;
                default:
                    errno = EINVAL;
                    return -1;
            }
			break;
		case VK_PIPE_VK_TX:
            /* read from receive side of virtual socket */
            rc = vk_vectoring_splice(&socket_ptr->rx.ring, vk_pipe_get_tx(&socket_ptr->rx_fd), -1);
            if (rc == -1) {
                vk_socket_handle_error(socket_ptr);
            }
            rc = 0;
            vk_socket_enqueue_read(socket_ptr);
            /* may have unblocked the other side */
            vk_socket_enqueue_readwriter(socket_ptr);

		    switch (socket_ptr->tx_fd.type) {
		        case VK_PIPE_OS_FD:
		            /* write to OS file descriptor */
                    rc = vk_vectoring_write(&socket_ptr->tx.ring, vk_pipe_get_fd(&socket_ptr->tx_fd));
                    if (rc == -1) {
                        vk_socket_handle_error(socket_ptr);
                    }
                    rc = 0;
                    vk_socket_enqueue_write(socket_ptr);
                    break;
                case VK_PIPE_VK_RX:
                    /* write to receive side of virtual socket */
                    rc = vk_vectoring_splice(vk_pipe_get_rx(&socket_ptr->tx_fd), &socket_ptr->tx.ring, -1);
                    if (rc == -1) {
                        vk_socket_handle_error(socket_ptr);
                    }
                    rc = 0;
                    vk_socket_enqueue_write(socket_ptr);
                    /* may have unblocked the other side */
                    vk_socket_enqueue_writereader(socket_ptr);
                    break;
                default:
                    errno = EINVAL;
                    return -1;
		    }
		    break;
        default:
            errno = EINVAL;
            return -1;
    }
	vk_socket_handle_block(socket_ptr);
	return 0;
}

/* satisfy VK_OP_READHUP */
ssize_t vk_socket_handle_readhup(struct vk_socket *socket_ptr) {
    vk_socket_dbg("readhup");
    switch (socket_ptr->rx_fd.type) {
        case VK_PIPE_OS_FD:
            vk_socket_enqueue_read(socket_ptr);
            break;
        case VK_PIPE_VK_TX:
            if (vk_vectoring_has_eof(vk_pipe_get_tx(&socket_ptr->rx_fd))) {
                vk_vectoring_clear_eof(vk_pipe_get_tx(&socket_ptr->rx_fd));
                vk_vectoring_mark_eof(&socket_ptr->rx.ring);
                vk_socket_enqueue_read(socket_ptr);
                /* may have unblocked the other side */
                vk_socket_enqueue_readwriter(socket_ptr);
            }
            break;
        case VK_PIPE_VK_RX:
        default:
            errno = EINVAL;
            return -1;
    }
    vk_socket_handle_block(socket_ptr);
    return 0;
}

/* satisfy VK_OP_HUP */
ssize_t vk_socket_handle_hup(struct vk_socket *socket_ptr) {
    vk_socket_dbg("hup");
	switch (socket_ptr->tx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_socket_enqueue_write(socket_ptr);
			break;
        case VK_PIPE_VK_RX:
            if (! vk_vectoring_has_eof(vk_socket_get_tx_vectoring(socket_ptr))) {
                /* readhup has taken the EOF -- unblock both ops */

                vk_socket_enqueue_write(socket_ptr);
                vk_socket_enqueue_writereader(socket_ptr);
            }
			break;
        case VK_PIPE_VK_TX:
		default:
			errno = EINVAL;
			return -1;
	}
	vk_socket_handle_block(socket_ptr);
	return 0;
}

int vk_socket_handle_tx_close(struct vk_socket *socket_ptr) {
    vk_socket_dbg("tx_close");
	switch (socket_ptr->tx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_socket_dbgf("closing write-side FD %i\n", vk_pipe_get_fd(&socket_ptr->tx_fd));
            vk_vectoring_mark_closed(&socket_ptr->tx.ring);
			vk_socket_enqueue_write(socket_ptr);
			break;
        case VK_PIPE_VK_RX:
        case VK_PIPE_VK_TX:
            vk_socket_enqueue_write(socket_ptr);
            break;
		default:
			errno = EINVAL;
			return -1;
	}
    vk_socket_handle_block(socket_ptr);
    if (
            vk_io_future_get_rx_closed(vk_block_get_ioft_rx_pre(vk_socket_get_block(socket_ptr)))
            && vk_io_future_get_tx_closed(vk_block_get_ioft_tx_pre(vk_socket_get_block(socket_ptr)))
            ) {
        /* only when both sides are closed does the poller physically close the fd_ptr then continue the coroutine */
        /* forward closed state from IO futures to the poller */
        vk_socket_enqueue_blocked(socket_ptr);
    } else {
        /* otherwise the closure is deferred, and we need to continue the coroutine here */
        /* vk_enqueue_run(socket_ptr->block.blocked_vk); -- execution loop implies this */
        vk_ready(socket_ptr->block.blocked_vk);
    }

    return 0;
}

int vk_socket_handle_rx_close(struct vk_socket *socket_ptr) {
    vk_socket_dbg("rx_close");
	switch (socket_ptr->rx_fd.type) {
		case VK_PIPE_OS_FD:
			vk_socket_dbgf("closing read-side FD %i\n", vk_pipe_get_fd(&socket_ptr->rx_fd));
            vk_vectoring_mark_closed(&socket_ptr->rx.ring);
            vk_socket_enqueue_read(socket_ptr);
			break;
        case VK_PIPE_VK_RX:
        case VK_PIPE_VK_TX:
            vk_socket_enqueue_read(socket_ptr);
            break;
		default:
			errno = EINVAL;
			return -1;
	}
    vk_socket_handle_block(socket_ptr);
    if (
            vk_io_future_get_rx_closed(vk_block_get_ioft_rx_pre(vk_socket_get_block(socket_ptr)))
            && vk_io_future_get_tx_closed(vk_block_get_ioft_tx_pre(vk_socket_get_block(socket_ptr)))
            ) {
        /* only when both sides are closed does the poller physically close the fd_ptr then continue the coroutine */
        /* forward closed state from IO futures to the poller */
        vk_socket_enqueue_blocked(socket_ptr);
    } else {
        /* otherwise the closure is deferred, and we need to continue the coroutine here */
        /* vk_enqueue_run(socket_ptr->block.blocked_vk); -- execution loop implies this */
        vk_ready(socket_ptr->block.blocked_vk);
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
	vk_proc_local_enqueue_blocked(vk_get_proc_local(socket_ptr->block.blocked_vk), socket_ptr);
}
int vk_socket_get_enqueued_blocked(struct vk_socket *socket_ptr) {
	return socket_ptr->blocked_enq;
}
void vk_socket_set_enqueued_blocked(struct vk_socket *socket_ptr, int blocked_enq) {
	socket_ptr->blocked_enq = blocked_enq;
}

/* handle socket event */
ssize_t vk_socket_handler(struct vk_socket *socket_ptr) {
	ssize_t rc;

	vk_socket_dbg("handling I/O");

	switch (socket_ptr->block.op) {
		case VK_OP_NONE:
			rc = 0;
			break;
        case VK_OP_FLUSH:
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
		case VK_OP_FORWARD:
		    rc = vk_socket_handle_forward(socket_ptr);
		    if (rc == -1) {
		        return -1;
            }
            break;
		case VK_OP_HUP:
			rc = vk_socket_handle_hup(socket_ptr);
			if (rc == -1) {
				return -1;
			}
			break;
        case VK_OP_READHUP:
            rc = vk_socket_handle_readhup(socket_ptr);
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

	vk_socket_dbg("handled I/O");

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


/* check EOF flag on socket -- more bytes may still be available to receive from socket */
int vk_socket_eof(struct vk_socket *socket_ptr) {
    return vk_vectoring_has_eof(vk_socket_get_rx_vectoring(socket_ptr));
}

/* no more bytes are available to receive from socket (but EOF may not be set) */
int vk_socket_empty(struct vk_socket *socket_ptr) {
    return vk_vectoring_is_empty(vk_socket_get_rx_vectoring(socket_ptr));
}

/* check EOF flag on socket, and that no more bytes are available to receive from socket */
int vk_socket_nodata(struct vk_socket *socket_ptr) {
    return vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_ptr));
}

/* clear EOF flag on socket */
void vk_socket_clear(struct vk_socket *socket_ptr) {
    vk_vectoring_clear_eof(vk_socket_get_rx_vectoring(socket_ptr));
}

/* check EOF flag on write-side */
int vk_socket_eof_tx(struct vk_socket *socket_ptr) {
    return vk_vectoring_has_eof(vk_socket_get_tx_vectoring(socket_ptr));
}

/* no more bytes are available to send from socket (but EOF may not be set) */
int vk_socket_empty_tx(struct vk_socket *socket_ptr) {
    return vk_vectoring_is_empty(vk_socket_get_tx_vectoring(socket_ptr));
}

/* check EOF flag on write side, and that no more bytes are to send */
int vk_socket_nodata_tx(struct vk_socket *socket_ptr) {
    return vk_vectoring_has_nodata(vk_socket_get_tx_vectoring(socket_ptr));
}

/* clear EOF flag on write side */
void vk_socket_clear_tx(struct vk_socket *socket_ptr) {
    vk_vectoring_clear_eof(vk_socket_get_tx_vectoring(socket_ptr));
}

/* whether the read pipe has nodata state */
int vk_socket_get_reader_nodata(struct vk_socket *socket_ptr) {
    struct vk_socket *reader_socket_ptr;
    struct vk_io_future *reader_ioft;

    /* check whether the writer has more to send */
    reader_socket_ptr = vk_socket_get_reader_socket(socket_ptr);
    if (reader_socket_ptr == NULL) {
        /* physical FD, so use the returned poll result */
        reader_ioft = vk_block_get_ioft_rx_ret(vk_socket_get_block(socket_ptr));
        /* `rx_closed` signals EOF, and `readable` signals bytes in rx */
        return vk_io_future_get_rx_closed(reader_ioft) && vk_io_future_get_readable(reader_ioft) == 0;
    } else {
        /* virtual socket, so check directly */
        return vk_socket_nodata_tx(reader_socket_ptr);
    }
}

/* may perform a readhup op */
int vk_socket_pollhup(struct vk_socket *socket_ptr) {
    int rc;

    if (!vk_socket_empty(socket_ptr)) {
        /* still need to read from this socket */
        vk_socket_dbg("pollhup = 0 -- current socket not empty");
        return 0;
    }

    rc = vk_socket_get_reader_nodata(socket_ptr);
    vk_socket_dbgf("pollhup = %i -- reader socket\n", rc);
    return rc;
}

int vk_socket_get_error(struct vk_socket *socket_ptr) {
    return socket_ptr->error;
}

struct vk_socket *vk_socket_next_blocked_socket(struct vk_socket *socket_ptr) {
	return SLIST_NEXT(socket_ptr, blocked_q_elem);
}

void vk_block_init(struct vk_block *block_ptr, char *buf, size_t len, int op)  {
	block_ptr->buf = buf;
	block_ptr->len = len;
	block_ptr->op  = op;
	block_ptr->copied = 0;
    vk_block_log("init");
}
ssize_t vk_block_commit(struct vk_block *block_ptr, ssize_t rc) {
	block_ptr->rc = rc;
	block_ptr->len -= rc;
	block_ptr->buf += rc;
	block_ptr->copied += rc;
    vk_block_logf("committed %zi for %zu/%zu\n", rc, block_ptr->copied, block_ptr->len + block_ptr->copied);
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
		case VK_OP_FORWARD: return "forward";
		case VK_OP_HUP: return "hup";
        case VK_OP_READHUP: return "readhup";
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

struct vk_io_future *vk_block_get_ioft_rx_pre(struct vk_block *block_ptr) {
    return &block_ptr->ioft_rx_pre;
}
struct vk_io_future *vk_block_get_ioft_tx_pre(struct vk_block *block_ptr) {
    return &block_ptr->ioft_tx_pre;
}
struct vk_io_future *vk_block_get_ioft_rx_ret(struct vk_block *block_ptr) {
    return &block_ptr->ioft_rx_ret;
}
struct vk_io_future *vk_block_get_ioft_tx_ret(struct vk_block *block_ptr) {
    return &block_ptr->ioft_tx_ret;
}


struct vk_thread *vk_block_get_vk(struct vk_block *block_ptr) {
	return block_ptr->blocked_vk;
}

void vk_block_set_vk(struct vk_block *block_ptr, struct vk_thread *blocked_vk) {
	block_ptr->blocked_vk = blocked_vk;
}
