#ifndef VK_IO_H
#define VK_IO_H

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
		if ( \
		    vk_block_commit( \
		        vk_socket_get_block(socket_ptr), \
		        vk_vectoring_send(\
		            vk_socket_get_tx_vectoring(socket_ptr), \
		            vk_block_get_buf(vk_socket_get_block(socket_ptr)), \
		            vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) \
		        ) \
		    ) == -1 \
		) { \
			vk_error(); \
		} \
		if (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
			vk_wait(socket_ptr); \
		} \
	} \
} while (0)

/* write into socket the formatted string, via the specified buffer line_arg (where the formatted result is stored, a buffer in *self that must survive blocks; rc_arg is how many bytes written -- errors if format does not fit into buffer (ENOBUFS), and on snprintf() error) */
#define vk_socket_writef(rc_arg, socket_ptr, line_arg, line_len_arg, fmt_arg, ...) do { \
    (rc_arg) = snprintf((line_arg), (line_len_arg), (fmt_arg), __VA_ARGS__); \
    if ((rc_arg) < 0) { \
        vk_error(); \
    } else if ((rc_arg) >= line_len_arg) { \
        errno = ENOBUFS; \
        vk_error(); \
    } \
    vk_write((line_arg), (rc_arg)); \
} while (0)

/* write into socket the specified literal string */
#define vk_socket_write_literal(socket_ptr, literal_arg) vk_socket_write(socket_ptr, literal_arg, sizeof(literal_arg) - 1)

/* flush write queue of socket (block until all is sent) */
#define vk_socket_flush(socket_ptr) do { \
	vk_block_init(vk_socket_get_block(socket_ptr), NULL, 0, VK_OP_FLUSH); \
	while (vk_vectoring_tx_len(vk_socket_get_tx_vectoring(socket_ptr)) > 0) { \
		vk_wait(socket_ptr); \
	} \
} while (0)

#define vk_socket_forward(rc_arg, socket_arg, len_arg) do { \
    vk_block_init(vk_socket_get_block(socket_arg), NULL, (len_arg), VK_OP_FORWARD); \
    while ( \
        !vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_arg)) \
    && vk_block_get_uncommitted(vk_socket_get_block(socket_arg)) > 0 \
    ) { \
        if ( \
            (                             \
                rc_arg = vk_block_commit( \
                    vk_socket_get_block(socket_arg), \
                    vk_vectoring_splice( \
                        vk_socket_get_tx_vectoring(socket_arg), \
                        vk_socket_get_rx_vectoring(socket_arg), \
                        vk_block_get_uncommitted(vk_socket_get_block(socket_arg)) \
                    )                                       \
                )                                           \
            ) == -1                                         \
        ) { \
            vk_error(); \
        } \
        if ( \
            !vk_vectoring_has_nodata(vk_socket_get_rx_vectoring(socket_arg)) \
        ||     vk_block_get_uncommitted(vk_socket_get_block(socket_arg)) > 0 \
        ) { \
            vk_wait(socket_arg); \
        } \
    } \
    rc_arg = vk_block_get_committed(vk_socket_get_block(socket_arg)); \
} while (0)

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

/* for edge-triggering, handle EAGAIN error, or when it is known that EAGAIN would happen */
#define vk_socket_again(socket_ptr) vk_socket_enqueue_blocked(socket_ptr)

#define vk_socket_readable(socket_ptr) do { \
	vk_socket_again(socket_ptr); \
	vk_block_init(vk_socket_get_block(socket_ptr), NULL, 1, VK_OP_READABLE); \
	while (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
		vk_block_set_uncommitted(vk_socket_get_block(socket_ptr), 0); \
		vk_wait(socket_ptr); \
	} \
} while (0)

#define vk_socket_writable(socket_ptr) do { \
	vk_socket_again(socket_ptr); \
	vk_block_init(vk_socket_get_block(socket_ptr), NULL, 1, VK_OP_WRITABLE); \
	while (vk_block_get_uncommitted(vk_socket_get_block(socket_ptr)) > 0) { \
		vk_block_set_uncommitted(vk_socket_get_block(socket_ptr), 0); \
		vk_wait(socket_ptr); \
	} \
} while (0)

#define vk_socket_accept(accepted_fd_arg, socket_ptr, accepted_ptr) do { \
    vk_socket_read(accepted_fd_arg, socket_ptr, (char *) accepted_ptr, sizeof (*accepted_ptr)); \
    if (accepted_fd_arg == -1) {                                         \
        break;                                                           \
    }                                                                    \
    accepted_fd_arg = vk_accepted_get_fd(accepted_ptr);                  \
} while (0)

#define vk_socket_accept2(accepted_fd_arg, socket_ptr, accepted_ptr) do { \
	do { \
		if (accepted_fd_arg = vk_accepted_accept() == -1) { \
			if (errno == EAGAIN) { \
				vk_socket_readable(socket_ptr); \
				continue; \
			} else { \
				vk_error(); \
				break; \
			} \
		} \
		break; \
	} while (1); \
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
#define vk_writef(rc_arg, line_arg, line_len, fmt_arg, ...) vk_socket_writef(       rc_arg, vk_get_socket(that), line_arg, line_len, fmt_arg, __VA_ARGS__)
#define vk_write_literal(literal_arg)         vk_socket_write_literal(   vk_get_socket(that), literal_arg)
#define vk_flush()                            vk_socket_flush(           vk_get_socket(that))
#define vk_tx_close()                         vk_socket_tx_close(        vk_get_socket(that))
#define vk_rx_close()                         vk_socket_rx_close(        vk_get_socket(that))
#define vk_again()                            vk_socket_again(           vk_get_socket(that))
#define vk_readable()                         vk_socket_readable(        vk_get_socket(that))
#define vk_writable()                         vk_socket_writable(        vk_get_socket(that))
#define vk_forward(rc_arg, len_arg)           vk_socket_forward(rc_arg, vk_get_socket(that), len_arg)
#define vk_accept(accepted_fd_arg, accepted_ptr) vk_socket_accept(accepted_fd_arg, vk_get_socket(that), accepted_ptr)

#endif
