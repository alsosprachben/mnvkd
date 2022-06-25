#ifndef VK_STATE_H
#define VK_STATE_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <poll.h>
#include <sys/queue.h>

#include "debug.h"
#include "vk_proc.h"
#include "vk_heap.h"
#include "vk_socket.h"
#include "vk_poll.h"
#include "vk_pipe.h"
#include "vk_future.h"

struct that;

enum VK_PROC_STAT {
	VK_PROC_RUN = 0, /* This coroutine needs to run at the start of its run queue. */
	VK_PROC_YIELD,   /* This coroutine needs to run at the end   of its run queue. */
	VK_PROC_LISTEN,  /* This coroutine is waiting for a vk_request(). */
	VK_PROC_WAIT,    /* This coroutine is waiting for I/O. */
	VK_PROC_ERR,     /* This coroutine needs to run, jumping to the vk_finally(). */
	VK_PROC_END,     /* This coroutine has ended. */
};
void vk_that_clear(                     struct that *that);
void vk_init(                           struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), struct vk_pipe *rx_fd, struct vk_pipe *tx_fd, const char *func_name, char *file, size_t line);
void vk_init_fds(                       struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), int rx_fd_arg, int tx_fd_arg,                 const char *func_name, char *file, size_t line);
void vk_init_child(struct that *parent, struct that *that,                           void (*func)(struct that *that),                                               const char *func_name, char *file, size_t line);

int vk_deinit(struct that *that);

size_t vk_alloc_size();
typedef void (*vk_func)(struct that *that);
vk_func vk_get_func(struct that *that);
void vk_set_func(struct that *that, void (*)(struct that *that));
const char *vk_get_func_name(struct that *that);
void vk_set_func_name(struct that *that, const char *func_name);
char *vk_get_file(struct that *that);
void vk_set_file(struct that *that, char *file);
int vk_get_line(struct that *that);
void vk_set_line(struct that *that, int line);
int vk_get_counter(struct that *that);
void vk_set_counter(struct that *that, int counter);
enum VK_PROC_STAT vk_get_status(struct that *that);
void vk_set_status(struct that *that, enum VK_PROC_STAT status);
int vk_get_error(struct that *that);
void vk_set_error(struct that *that, int error);
int vk_get_error_counter(struct that *that);
void vk_set_error_counter(struct that *that, int error_counter);
struct vk_proc *vk_get_proc(struct that *that);
void vk_set_proc(struct that *that, struct vk_proc *proc_ptr);
void *vk_get_self(struct that *that);
void vk_set_self(struct that *that, void *self);
struct vk_socket *vk_get_socket(struct that *that);
void vk_set_socket(struct that *that, struct vk_socket *socket_ptr);
struct vk_socket *vk_get_waiting_socket(struct that *that);
void vk_set_waiting_socket(struct that *that, struct vk_socket *waiting_socket_ptr);
struct vk_future *vk_get_future(struct that *that);
void vk_set_future(struct that *that, struct vk_future *ft_ptr);
struct vk_pipe *vk_get_rx_fd(struct that *that);
void vk_set_rx_fd(struct that *that, struct vk_pipe *rx_fd);
struct vk_pipe *vk_get_tx_fd(struct that *that);
void vk_set_tx_fd(struct that *that, struct vk_pipe *tx_fd);
void vk_enqueue_run(struct that *that);
int vk_get_enqueued_run(struct that *that);
void vk_set_enqueued_run(struct that *that, int run_enq);

/* next coroutine in the proc run queue */
struct that *vk_next_run_vk(struct that *that);

/* whether coroutine status is VK_PROC_END */
int vk_is_completed(struct that *that);

/* whether coroutine status is VK_PROC_RUN or VK_PROC_ERR */
int vk_is_ready(struct that *that);

/* whether coroutine status is VK_PROC_YIELD:
	a temporary status for enqueued coroutines to set back to VK_PROC_RUN after breaking out of the ready loop.
*/
int vk_is_yielding(struct that *that);

/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct that *that);

ssize_t vk_unblock(struct that *that);

void vk_deblock_waiting_socket(struct that *that);
void vk_deblock_socket(struct that *that);
void vk_derun(struct that *that);

int vk_copy_arg(struct that *that, void *src, size_t n);

/* primary coroutine */
#define VK_INIT(that, proc_ptr, vk_func, rx_fd_arg, tx_fd_arg) \
	vk_init_fds(that, proc_ptr, vk_func, rx_fd_arg, tx_fd_arg, #vk_func, __FILE__, __LINE__)

#define VK_INIT_CHILD(parent, that, vk_func) \
	vk_init_child(parent, that, vk_func, #vk_func, __FILE__, __LINE__)

/* coroutine-scoped for responder */
#define vk_child(child, vk_func) VK_INIT_CHILD(that, child, vk_func)

/* coroutine-scoped for accepted socket into new heap */
#define vk_accepted(there, vk_func, rx_fd_arg, tx_fd_arg) VK_INIT(there, that->proc_ptr, vk_func, rx_fd_arg, tx_fd_arg)

/* set up pipeline with parent */
/* child coroutine that takes over writes from the parent, connecting via internal pipe the parent's writes to the child's reads */
/* lifecycle: parent read FD -> parent write pipe -> child read pipe -> child write FD */
/* This allows the responder to start writing before the reading is complete. */
#define vk_pipeline(parent) do { \
	/* - bind parent tx to rx */ \
	vk_pipe_init_rx(vk_socket_get_tx_fd(vk_get_socket(parent)), vk_get_socket(that)); \
	/* - bind rx to parent tx */ \
	vk_pipe_init_tx(vk_socket_get_rx_fd(vk_get_socket(that)), vk_get_socket(parent)); \
} while (0)

#define vk_begin_pipeline(parent_ft_ptr) \
	vk_begin(); \
	vk_get_request(parent_ft_ptr); \
	vk_pipeline((parent_ft_ptr)->vk); \
	vk_future_resolve(parent_ft_ptr, 0); \
	vk_respond(parent_ft_ptr)

#define vk_depipeline(parent) do { \
	 \
} while (0)

/* allocation via the heap */

/* allocate an array of the pointer from the micro-heap as a stack */
#define vk_calloc(val_ptr, nmemb) \
	(val_ptr) = vk_heap_push(vk_proc_get_heap(vk_get_proc(that)), (nmemb), sizeof (*(val_ptr))); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

#define vk_calloc_size(val_ptr, size, nmemb) \
	(val_ptr) = vk_heap_push(vk_proc_get_heap(vk_get_proc(that)), (nmemb), size); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

/* de-allocate the last array from the micro-heap stack */
#define vk_free()                                          \
	rc = vk_heap_pop(vk_proc_get_heap(vk_get_proc(that))); \
	if (rc == -1) {                                        \
		if (vk_get_counter(that) == -2) {                         \
			vk_perror("vk_free");                          \
		} else {                                           \
			vk_error();                                    \
		}                                                  \
	}


/* state machine macros */

/* continue RUN state, branching to blocked entrypoint, or error entrypoint, and allocate self */
#define vk_begin()                            \
{                                             \
	struct vk_socket *__vk_socket_ptr = NULL; \
	self = vk_get_self(that);                 \
	if (vk_get_status(that) == VK_PROC_ERR) { \
		vk_set_counter(that, -2);             \
		vk_set_status(that, VK_PROC_RUN);     \
	}                                         \
	switch (vk_get_counter(that)) {           \
		case -1:                              \
			vk_set_self(that, self);          \
			vk_set_file(that, __FILE__);      \
			vk_set_func_name(that, __func__); \
			vk_calloc(self, 1);               \
			vk_calloc_size(__vk_socket_ptr, vk_socket_size(__vk_socket_ptr), 1); \
			vk_set_socket(that, __vk_socket_ptr); \
			vk_socket_init(vk_get_socket(that), that, vk_get_rx_fd(that), vk_get_tx_fd(that))

/* de-allocate self and set END state */
#define vk_end()                              \
		default:                              \
			vk_deblock_waiting_socket(that);  \
			vk_deblock_socket(that);          \
			vk_free(); /* self */             \
			vk_free(); /* socket */           \
			vk_set_line(that, __LINE__);      \
			vk_set_status(that, VK_PROC_END); \
			vk_derun(that);                   \
	}                                         \
}                                             \
return

/* stop coroutine in specified run state */
#define vk_yield(s) do {               \
	vk_set_line(that, __LINE__);       \
	vk_set_counter(that, __COUNTER__); \
	vk_set_status(that, s);            \
	return;                            \
	case __COUNTER__ - 1:;             \
} while (0)                            \

/* schedule the specified coroutine to run */
#define vk_play(there) vk_enqueue_run(there)

/*
 * stop coroutine in YIELD state,
 * which will break out of execution,
 * and immediately be set back to RUN state,
 * ready for vk_play() add it back to the run queue.
 */
#define vk_pause() do {      \
	vk_yield(VK_PROC_YIELD); \
} while (0)

/* schedule the callee to run, then stop the coroutine in YIELD state */
#define vk_call(there) do { \
	vk_play(there);         \
	vk_pause();             \
} while (0)

/* call coroutine, passing pointer to message to it, but without a return */
#define vk_spawn(there, return_ft_ptr, send_msg) do { \
	(return_ft_ptr)->vk = that;                       \
	(return_ft_ptr)->msg = (void *) (send_msg);       \
	vk_set_future((there), (return_ft_ptr));          \
	vk_play(there);                                   \
} while (0)

/* call coroutine, passing pointers to messages to and from it */
#define vk_request(there, return_ft_ptr, send_msg, recv_msg) do { \
	(return_ft_ptr)->vk = that;                 \
	(return_ft_ptr)->msg = (void *) (send_msg); \
	vk_set_future((there), (return_ft_ptr));    \
	vk_call(there);                             \
	recv_msg = (return_ft_ptr)->msg;            \
} while (0)

/* receive message */
#define vk_get_request(recv_ft_ptr) do {           \
	(recv_ft_ptr)->vk  = vk_get_future(that)->vk;  \
	(recv_ft_ptr)->msg = vk_get_future(that)->msg; \
} while (0)

/* pause coroutine for request, receiving message on play */
#define vk_listen(recv_ft_ptr) do { \
	vk_yield(VK_PROC_LISTEN);       \
	vk_get_request(recv_ft_ptr);    \
} while (0)

/* play the coroutine of the resolved future */
#define vk_respond(send_ft_ptr) do { \
	vk_play((send_ft_ptr)->vk);      \
} while (0)

/* stop coroutine in WAIT state, marking blocked socket */
#define vk_wait(socket_ptr) do {                \
	vk_set_waiting_socket(that, (socket_ptr));  \
	vk_block_set_vk(vk_socket_get_block(socket_ptr), that); \
	vk_yield(VK_PROC_WAIT);                     \
	vk_set_waiting_socket(that, NULL);          \
	/*(socket_arg).block.blocked_vk = NULL;*/   \
} while (0)

/*
 * error handling
 */

/* entrypoint for errors */
#define vk_finally() do {           \
	case -2:                        \
		errno = vk_get_error(that); \
} while (0)

/* restart coroutine in ERR state, marking error, continuing at cr_finally() */
#define vk_raise(e) do {                     \
	vk_set_error(that, e);                   \
	vk_set_error_counter(that, vk_get_counter(that)); \
	vk_play(that);                       \
	vk_yield(VK_PROC_ERR);               \
} while (0)

/* restart target coroutine in ERR state, marking error, continuing at cr_finally() */
#define vk_raise_at(there, e) do { \
	vk_set_error(there, e); \
	vk_set_error_counter(there, vk_get_counter(there)); \
	vk_set_status(there, VK_PROC_ERR); \
	vk_play(there); \
} while (0)

/* restart coroutine in ERR state, marking errno as error, continuing at cr_finally() */
#define vk_error() vk_raise(errno)

/* restart target coroutine in ERR state, marking errno as error, continuing at cr_finally() */
#define vk_error_at(there) vk_raise_at(there, errno)

/* restart coroutine in RUN state, clearing the error, continuing back where at cr_raise()/cr_error() */
#define vk_lower() do {                      \
	vk_set_error(that, 0);                     \
	vk_set_counter(that, vk_get_error_counter(that)); \
	vk_play(that);                       \
	vk_yield(VK_PROC_RUN);               \
} while (0)

/*
 * I/O
 */

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
#define vk_socket_hup(socket_ptr) vk_vectoring_mark_eof(vk_socket_get_tx_vectoring(socket_ptr))

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
