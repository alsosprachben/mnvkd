#ifndef VK_IO_H
#define VK_IO_H

/* read from FD into socket's read (rx) queue */
#define vk_socket_fd_read(rc, socket_ptr, d) do { \
	rc = vk_vectoring_read(vk_socket_get_rx_vectoring(socket_ptr), d); \
	vk_wait(socket_ptr); \
} while (0)

/* write to FD from socket's write (tx) queue */
#define vk_socket_fd_write(rc, socket_ptr, d) do { \
	rc = vk_vectoring_write(vk_socket_get_tx_vectoring(socket_ptr), d); \
	vk_wait(socket_ptr); \
} while (0)

/* read from socket into specified buffer of specified length */
#define vk_socket_read(rc_arg, socket_ptr, buf_arg, len_arg) do { \
	vk_block_init(vk_socket_get_block(socket_ptr), (buf_arg), (len_arg), VK_OP_READ); \
	while (!vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_ptr)) && vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
		if (vk_block_commit(vk_socket_get_block(socket_ptr), vk_vectoring_recv(vk_socket_get_rx_vectoring(socket_ptr), vk_block_get_buf(vk_socket_get_block(socket_ptr)), vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)))) == -1) { \
			vk_error(); \
		} \
		if (!vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_ptr)) && vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
			vk_wait(socket_ptr); \
		} \
	} \
	rc_arg = vk_block_get_committed(vk_socket_get_block(socket_ptr)); \
} while (0)

/* read a line from socket into specified buffer of specified length -- up to specified length, leaving remnants of line if exceeded */
#define vk_socket_readline(rc_arg, socket_ptr, buf_arg, len_arg) do { \
	vk_block_init(vk_socket_get_block(socket_ptr), (buf_arg), (len_arg), VK_OP_READ); \
	while (!vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_ptr)) && vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0 && (vk_block_get_committed(vk_socket_get_block(socket_ptr)) == 0 || vk_block_get_buf(vk_socket_get_block(socket_ptr))[vk_block_get_committed(vk_socket_get_block(socket_ptr)) - 1] != '\n')) { \
		vk_block_set_uncommitted(vk_socket_get_block(socket_ptr), vk_vectoring_tx_line_request(vk_socket_get_rx_vectoring(socket_ptr), vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)))); \
		if (vk_block_commit(vk_socket_get_block(socket_ptr), vk_vectoring_recv(vk_socket_get_rx_vectoring(socket_ptr), vk_block_get_buf(vk_socket_get_block(socket_ptr)), vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)))) == -1) { \
			vk_error(); \
		} \
		if (!vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_ptr)) && vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0 && vk_block_get_buf(vk_socket_get_block(socket_ptr))[vk_block_get_committed(vk_socket_get_block(socket_ptr)) - 1] != '\n') { \
			vk_wait(socket_ptr); \
		} \
	} \
	rc_arg = vk_block_get_committed(vk_socket_get_block(socket_ptr)); \
} while (0)

/* check EOF flag on socket -- more bytes may still be available to receive from socket */
#define vk_socket_eof(socket_ptr) vk_vectoring_has_eof(vk_socket_get_rx_vectoring(socket_ptr))

/* check EOF flag on socket, and that no more bytes are available to receive from socket */
#define vk_socket_nodata(socket_ptr) vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_ptr))

/* clear EOF flag on socket */
#define vk_socket_clear(socket_ptr) vk_vectoring_clear_eof(vk_socket_get_rx_vectoring(socket_ptr))

/* hang-up transmit socket (set EOF on the read side of the consumer) */
#define vk_socket_hup(socket_ptr) do { \
	vk_socket_flush(socket_ptr); \
	vk_block_init(vk_socket_get_block(socket_ptr), NULL, 1, VK_OP_HUP); \
	while (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
		if (vk_block_commit(vk_socket_get_block(socket_ptr), vk_vectoring_mark_eof(vk_socket_get_tx_vectoring(socket_ptr))) == -1) { \
			vk_error(); \
		} \
		if (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
			vk_wait(socket_ptr); \
		} \
	} \
} while (0)

/* socket is hanged-up (EOF is set on the read side of the consumer) */
#define vk_socket_hanged(socket_ptr) vk_vectoring_has_eof(vk_socket_get_tx_vectoring(socket_ptr))

/* write into socket the specified buffer of specified length */
#define vk_socket_write(socket_ptr, buf_arg, len_arg) do { \
	vk_block_init(vk_socket_get_block(socket_ptr), (buf_arg), (len_arg), VK_OP_WRITE); \
	while (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
		if (vk_block_commit(vk_socket_get_block(socket_ptr), vk_vectoring_send(vk_socket_get_tx_vectoring(socket_ptr), vk_block_get_buf(vk_socket_get_block(socket_ptr)), vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)))) == -1) { \
			vk_error(); \
		} \
		if (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
			vk_wait(socket_ptr); \
		} \
	} \
} while (0)

/* flush write queue of socket (block until all is sent) */
#define vk_socket_flush(socket_ptr) do { \
	vk_block_init(vk_socket_get_block(socket_ptr), NULL, 0, VK_OP_FLUSH); \
	while (vk_vectoring_tx_len(vk_socket_get_tx_vectoring(socket_ptr)) > 0) { \
		vk_wait(socket_ptr); \
	} \
} while (0)

/* write into socket, splicing reads from the specified socket the specified length */
#define vk_socket_write_splice(rc_arg, tx_socket_ptr, rx_socket_ptr, len_arg) do { \
	vk_block_init(vk_socket_get_block(tx_socket_ptr), NULL, (len_arg), VK_OP_WRITE); \
	vk_block_init(vk_socket_get_block(rx_socket_ptr), NULL, (len_arg), VK_OP_READ); \
	while (!vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(rx_socket_ptr)) && vk_block_get_uncommitted(vk_socket_get_block(tx_socket_ptr)) > 0) { \
		if (vk_block_commit(vk_socket_get_block(tx_socket_ptr), vk_block_commit(vk_socket_get_block(rx_socket_ptr), vk_vectoring_recv_splice(vk_socket_get_rx_vectoring(rx_socket_ptr), vk_socket_get_tx_vectoring(tx_socket_ptr), vk_block_get_committed(vk_socket_get_block(tx_socket_ptr))))) == -1) { \
			vk_error(); \
		} \
		if (!vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(rx_socket_ptr)) && vk_block_get_uncommitted(vk_socket_get_block(rx_socket_ptr)) > 0) { \
			vk_wait(rx_socket_ptr); \
		} \
		if (vk_block_get_uncommitted(vk_socket_get_block(tx_socket_ptr)) > 0) { \
			vk_wait(tx_socket_ptr); \
		} \
	} \
	rc_arg = vk_block_get_committed(vk_socket_get_block(tx_socket_ptr)); \
} while (0)

/* read from socket, splicing writes into the specified socket the specified length */
#define vk_socket_read_splice(rc_arg, rx_socket_ptr, tx_socket_ptr, len_arg) vk_socket_write_splice(rc_arg, tx_socket_ptr, rx_socket_ptr, len_arg)

#define vk_socket_tx_close(socket_ptr) do { \
	vk_block_init(vk_socket_get_block(socket_ptr), NULL, 1, VK_OP_TX_CLOSE); \
	while (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
		vk_block_set_uncommitted(vk_socket_get_block(socket_ptr), 0); \
		vk_wait(socket_ptr); \
	} \
} while (0)

#define vk_socket_rx_close(socket_ptr) do { \
	vk_block_init(vk_socket_get_block(socket_ptr), NULL, 1, VK_OP_RX_CLOSE); \
	while (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
		vk_block_set_uncommitted(vk_socket_get_block(socket_ptr), 0); \
		vk_wait(socket_ptr); \
	} \
} while (0)

#include <sys/socket.h>
#include <fcntl.h>
#define vk_socket_accept(accepted_fd_arg, socket_ptr, accepted_ptr) do { \
	vk_socket_enqueue_blocked(vk_get_socket(that)); \
	vk_wait(vk_get_socket(that)); \
	*vk_accepted_get_address_len_ptr(accepted_ptr) = vk_accepted_get_address_storage_len(accepted_ptr); \
	if ((accepted_fd_arg = accept(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), vk_accepted_get_address(accepted_ptr), vk_accepted_get_address_len_ptr(accepted_ptr))) == -1) { \
		vk_error(); \
	} \
	fcntl(accepted_fd_arg, F_SETFL, O_NONBLOCK); \
	if (vk_accepted_set_address_str(accepted_ptr) == NULL) { \
		vk_error(); \
	} \
	if (vk_accepted_set_port_str(accepted_ptr) == -1) { \
		vk_error(); \
	} \
} while (0)

/* above socket operations, but applying to the coroutine's standard socket */
#define vk_read(    rc_arg, buf_arg, len_arg) vk_socket_read(    rc_arg, vk_get_socket(that), buf_arg, len_arg)
#define vk_readline(rc_arg, buf_arg, len_arg) vk_socket_readline(rc_arg, vk_get_socket(that), buf_arg, len_arg)
#define vk_eof()                              vk_socket_eof(             vk_get_socket(that))
#define vk_clear()                            vk_socket_clear(           vk_get_socket(that))
#define vk_nodata()                           vk_socket_nodata(          vk_get_socket(that))
#define vk_hup()                              vk_socket_hup(             vk_get_socket(that))
#define vk_hanged()                           vk_socket_hanged(          vk_get_socket(that))
#define vk_write(buf_arg, len_arg)            vk_socket_write(           vk_get_socket(that), buf_arg, len_arg)
#define vk_flush()                            vk_socket_flush(           vk_get_socket(that))
#define vk_tx_close()                         vk_socket_tx_close(        vk_get_socket(that))
#define vk_rx_close()                         vk_socket_rx_close(        vk_get_socket(that))
#define vk_read_splice( rc_arg, socket_arg, len_arg) vk_socket_read_splice( rc_arg, vk_get_socket(that), socket_arg, len_arg) 
#define vk_write_splice(rc_arg, socket_arg, len_arg) vk_socket_write_splice(rc_arg, vk_get_socket(that), socket_arg, len_arg) 
#define vk_accept(accepted_fd_arg, accepted_ptr) vk_socket_accept(accepted_fd_arg, vk_get_socket(that), accepted_ptr)

#endif