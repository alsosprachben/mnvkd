#ifndef VK_STATE_H
#define VK_STATE_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <poll.h>
#include <sys/queue.h>

#include "vk_proc.h"
#include "vk_heap_s.h"
#include "vk_heap.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "vk_poll.h"
#include "vk_pipe.h"

struct that;
struct future;

/* Inter-coroutine message passing, between the coroutines within a single heap. */
struct future {
	struct future *next; /* for reentrance */
	struct that *vk;
	intptr_t msg;
	int error; /* errno */
};

#define future_get(ft)     ((void *) (ft).msg)
#define future_bind(ft, vk_ptr)     ((ft).vk = (vk_ptr)
#define future_resolve(ft, msg_arg) ((ft).msg = (msg_arg))

/* The coroutine process struct. The coroutine function's state is the pointer `self` to its heap. */
struct that {
	void (*func)(struct that *that);
	const char *func_name;
	char *file;
	int line;
	int counter;
	enum VK_PROC_STAT {
		VK_PROC_RUN = 0, /* This coroutine needs to run at the start of its run queue. */
		VK_PROC_YIELD,   /* This coroutine needs to run at the end   of its run queue. */
		VK_PROC_LISTEN,  /* This coroutine is waiting for a vk_request(). */
		VK_PROC_WAIT,    /* This coroutine is waiting for I/O. */
		VK_PROC_ERR,     /* This coroutine needs to run, jumping to the vk_finally(). */
		VK_PROC_END,     /* This coroutine has ended. */
	} status;
	int error;
	int error_counter;
	struct vk_proc *proc_ptr;
	void *self;
	struct vk_socket *socket_ptr;
	struct vk_socket *waiting_socket_ptr;
	struct future *ft_ptr;
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	SLIST_ENTRY(that) run_q_elem;
	int run_enq;
	SLIST_ENTRY(that) blocked_q_elem;
	int blocked_enq;
};
void vk_init(                           struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), struct vk_pipe rx_fd, struct vk_pipe tx_fd, const char *func_name, char *file, size_t line);
void vk_init_fds(                       struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), int rx_fd_arg, int tx_fd_arg,               const char *func_name, char *file, size_t line);
void vk_init_child(struct that *parent, struct that *that,                           void (*func)(struct that *that),                                             const char *func_name, char *file, size_t line);

int vk_deinit(struct that *that);
struct vk_proc *vk_get_proc(struct that *that);
void vk_enqueue_run(struct that *that);
void vk_enqueue_blocked(struct that *that);

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

/* primary coroutine */
#define VK_INIT(that, proc_ptr, vk_func, rx_fd_arg, tx_fd_arg) \
	vk_init_fds(that, proc_ptr, vk_func, rx_fd_arg, tx_fd_arg, #vk_func, __FILE__, __LINE__)

/* child coroutine that takes over writes from the parent, connecting via internal pipe the parent's writes to the child's reads */
/* lifecycle: parent read FD -> parent write pipe -> child read pipe -> child write FD */
/* This allows the responder to start writing before the reading is complete. */

#define VK_INIT_CHILD(parent, that, vk_func) \
	vk_init_child(parent, that, vk_func, #vk_func, __FILE__, __LINE__)

/* coroutine-scoped for responder */
#define vk_child(child, vk_func) VK_INIT_CHILD(that, child, vk_func)

/* coroutine-scoped for accepted socket into new heap */
#define vk_accepted(parent, vk_func, rx_fd_arg, tx_fd_arg) VK_INIT(parent, vk_func, rx_fd_arg, tx_fd_arg)

#define vk_procdump(that, tag)                      \
	fprintf(                                    \
			stderr,                     \
			"tag: %s\n"                 \
			"line: %s:%i\n"             \
			"counter: %i\n"             \
			"status: %i\n"              \
			"error: %i\n"               \
			"msg: %p\n"                 \
			"\n"                        \
			,                           \
			(tag),                      \
			(that)->file, (that)->line, \
			(that)->counter,            \
			(that)->status,             \
			(that)->error,              \
			(void *) (that)->msg        \
	)

/* allocation via the heap */

/* allocate an array of the pointer from the micro-heap as a stack */
#define vk_calloc(val_ptr, nmemb) \
	(val_ptr) = vk_heap_push(vk_proc_get_heap(vk_get_proc(that)), (nmemb), sizeof (*(val_ptr))); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

/* de-allocate the last array from the micro-heap stack */
#define vk_free()                                          \
	rc = vk_heap_pop(vk_proc_get_heap(vk_get_proc(that))); \
	if (rc == -1) {                                        \
		if (that->counter == -2) {                         \
			vk_perror("vk_free");                          \
		} else {                                           \
			vk_error();                                    \
		}                                                  \
	}


/* state machine macros */

/* continue RUN state, branching to blocked entrypoint, or error entrypoint, and allocate self */
#define vk_begin()                      \
self = that->self;                      \
if (that->status == VK_PROC_ERR) {      \
	that->counter = -2;                 \
	that->status = VK_PROC_RUN;         \
}                                       \
switch (that->counter) {                \
	case -1:                            \
		that->file = __FILE__;          \
		that->func_name = __func__;     \
		vk_calloc(self, 1);             \
		vk_calloc(that->socket_ptr, 1); \
		that->self = self;              \
		vk_socket_init(that->socket_ptr, that, that->rx_fd, that->tx_fd)

/* de-allocate self and set END state */
#define vk_end()                    \
	default:                        \
		vk_free(); /* self */       \
		vk_free(); /* socket */     \
		that->status = VK_PROC_END; \
}                                   \
return

/* stop coroutine in specified run state */
#define vk_yield(s) do {             \
	that->line = __LINE__;       \
	that->counter = __COUNTER__; \
	that->status = s;            \
	return;                      \
	case __COUNTER__ - 1:;       \
} while (0)                          \

/* schedule the specified coroutine to run */
#define vk_play(there) vk_enqueue_run(there)

/* stop coroutine in YIELD state, which will defer the coroutine to the end of the run queue */
#define vk_pause() do {        \
	vk_yield(VK_PROC_YIELD); \
} while (0)

/* schedule the callee to run, then stop the coroutine in YIELD state */
#define vk_call(there) do { \
	vk_play(there);     \
	vk_pause();         \
} while (0)

/* call coroutine, passing pointer to message to it, but without a return */
#define vk_spawn(there, return_ft, send_msg) do { \
	(return_ft).vk = that;                    \
	(return_ft).msg = (intptr_t) (send_msg);  \
	(there)->ft_ptr = &(return_ft);           \
	vk_play(there);                           \
} while (0)

/* call coroutine, passing pointers to messages to and from it */
#define vk_request(there, return_ft, send_msg, recv_msg) do { \
	(return_ft).vk = that;                   \
	(return_ft).msg = (intptr_t) (send_msg); \
	(there)->ft_ptr = &(return_ft);          \
	vk_call(there);                          \
	recv_msg = (return_ft).msg;              \
} while (0)

/* receive message */
#define vk_get_request(recv_ft) do {       \
	(recv_ft).vk  = that->ft_ptr->vk;  \
	(recv_ft).msg = that->ft_ptr->msg; \
} while (0)

/* pause coroutine for request, receiving message on play */
#define vk_listen(recv_ft) do {            \
	vk_yield(VK_PROC_LISTEN);          \
	vk_get_request(recv_ft);           \
} while (0)

/* play the coroutine of the resolved future */
#define vk_respond(send_ft) do { \
	vk_play((send_ft).vk);   \
} while (0)

/* stop coroutine in WAIT state, marking blocked socket */
#define vk_wait(socket_arg) do {                  \
	that->waiting_socket_ptr = &(socket_arg); \
	(socket_arg).block.blocked_vk = that;     \
	vk_yield(VK_PROC_WAIT);                   \
	that->waiting_socket_ptr = NULL;          \
	/*(socket_arg).block.blocked_vk = NULL;*/     \
} while (0)

/*
 * error handling
 */

/* entrypoint for errors */
#define vk_finally() do {            \
	case -2:                     \
		errno = that->error; \
} while (0)

/* restart coroutine in ERR state, marking error, continuing at cr_finally() */
#define vk_raise(e) do {                     \
	that->error = e;                     \
	that->error_counter = that->counter; \
	vk_play(that);                       \
	vk_yield(VK_PROC_ERR);               \
} while (0)

/* restart coroutine in ERR state, marking errno as error, continuing at cr_finally() */
#define vk_error() vk_raise(errno)

/* restart coroutine in RUN state, clearing the error, continuing back where at cr_raise()/cr_error() */
#define vk_lower() do {                      \
	that->error = 0;                     \
	that->counter = that->error_counter; \
	vk_play(that);                       \
	vk_yield(VK_PROC_RUN);               \
} while (0)

/* get errno value set by vk_raise() */
#define vk_get_error() (that->error)

/*
 * I/O
 */

/* read from FD into socket's read (rx) queue */
#define vk_socket_fd_read(rc, socket_arg, d) do { \
	rc = vk_vectoring_read(&(socket_arg).rx.ring, d); \
	vk_wait(socket_arg); \
} while (0)

/* write to FD from socket's write (tx) queue */
#define vk_socket_fd_write(rc, socket_arg, d) do { \
	rc = vk_vectoring_write(&(socket_arg).tx.ring, d); \
	vk_wait(socket_arg); \
} while (0)

/* read from socket into specified buffer of specified length */
#define vk_socket_read(rc_arg, socket_arg, buf_arg, len_arg) do { \
	(socket_arg).block.buf    = (buf_arg); \
	(socket_arg).block.len    = (len_arg); \
	(socket_arg).block.op     = VK_OP_READ; \
	(socket_arg).block.copied = 0; \
	while (!vk_vectoring_has_nodata(&(socket_arg).rx.ring) && (socket_arg).block.len > 0) { \
		(socket_arg).block.rc = vk_vectoring_recv(&(socket_arg).rx.ring, (socket_arg).block.buf, (socket_arg).block.len); \
		if ((socket_arg).block.rc == -1) { \
			vk_error(); \
		} else if ((socket_arg).block.rc == 0) { \
		} else { \
			(socket_arg).block.len    -= (size_t) (socket_arg).block.rc; \
			(socket_arg).block.buf    += (size_t) (socket_arg).block.rc; \
			(socket_arg).block.copied += (size_t) (socket_arg).block.rc; \
		} \
		if (!vk_vectoring_has_nodata(&(socket_arg).rx.ring) && (socket_arg).block.len > 0) { \
			vk_wait(socket_arg); \
		} \
	} \
	rc_arg = (socket_arg).block.copied; \
} while (0);

/* read a line from socket into specified buffer of specified length -- up to specified length, leaving remnants of line if exceeded */
#define vk_socket_readline(rc_arg, socket_arg, buf_arg, len_arg) do { \
	(socket_arg).block.buf    = (buf_arg); \
	(socket_arg).block.len    = (len_arg); \
	(socket_arg).block.op     = VK_OP_READ; \
	(socket_arg).block.copied = 0; \
	while (!vk_vectoring_has_nodata(&(socket_arg).rx.ring) && (socket_arg).block.len > 0 && ((socket_arg).block.copied == 0 || (socket_arg).block.buf[(socket_arg).block.copied - 1] != '\n')) { \
		(socket_arg).block.len = vk_vectoring_tx_line_request(&(socket_arg).rx.ring, (socket_arg).block.len); \
		(socket_arg).block.rc = vk_vectoring_recv(&(socket_arg).rx.ring, (socket_arg).block.buf, (socket_arg).block.len); \
		if ((socket_arg).block.rc == -1) { \
			vk_error(); \
		} else if ((socket_arg).block.rc == 0) { \
		} else { \
			(socket_arg).block.len    -= (size_t) (socket_arg).block.rc; \
			(socket_arg).block.buf    += (size_t) (socket_arg).block.rc; \
			(socket_arg).block.copied += (size_t) (socket_arg).block.rc; \
		} \
		if (!vk_vectoring_has_nodata(&(socket_arg).rx.ring) && (socket_arg).block.len > 0 && (socket_arg).block.buf[(socket_arg).block.copied - 1] != '\n') { \
			vk_wait(socket_arg); \
		} \
	} \
	rc_arg = (socket_arg).block.copied; \
} while (0);

/* check EOF flag on socket -- more bytes may still be available to receive from socket */
#define vk_socket_eof(socket_arg) vk_vectoring_has_eof(&(socket_arg).rx.ring)

/* check EOF flag on socket, and that no more bytes are available to receive from socket */
#define vk_socket_nodata(socket_arg) vk_vectoring_has_nodata(&(socket_arg).rx.ring)

/* clear EOF flag on socket */
#define vk_socket_clear(socket_arg) vk_vectoring_clear_eof(&(socket_arg).rx.ring)

/* hang-up transmit socket (set EOF on the read side of the consumer) */
#define vk_socket_hup(socket_arg) vk_vectoring_mark_eof(&(socket_arg).tx.ring)

/* write into socket the specified buffer of specified length */
#define vk_socket_write(socket_arg, buf_arg, len_arg) do { \
	(socket_arg).block.buf    = buf_arg; \
	(socket_arg).block.len    = len_arg; \
	(socket_arg).block.op     = VK_OP_WRITE; \
	(socket_arg).block.copied = 0; \
	while ((socket_arg).block.len > 0) { \
		(socket_arg).block.rc = vk_vectoring_send(&(socket_arg).tx.ring, (socket_arg).block.buf, (socket_arg).block.len); \
		if ((socket_arg).block.rc == -1) { \
			vk_error(); \
		} else { \
			(socket_arg).block.len    -= (size_t) (socket_arg).block.rc; \
			(socket_arg).block.buf    += (size_t) (socket_arg).block.rc; \
			(socket_arg).block.copied += (size_t) (socket_arg).block.rc; \
		} \
		if ((socket_arg).block.len > 0) { \
			vk_wait(socket_arg); \
		} \
	} \
} while (0);

/* flush write queue of socket (block until all is sent) */
#define vk_socket_flush(socket_arg) do { \
	(socket_arg).block.buf = NULL; \
	(socket_arg).block.len = 0; \
	(socket_arg).block.op  = VK_OP_FLUSH; \
	(socket_arg).block.rc  = 0; \
	while (vk_vectoring_tx_len(&(socket_arg).tx.ring) > 0) { \
		vk_wait(socket_arg); \
	} \
} while (0);

/* write into socket, splicing reads from the specified socket the specified length */
#define vk_socket_write_splice(rc_arg, tx_socket_arg, rx_socket_arg, len_arg) do { \
	(tx_socket_arg).block.buf    = NULL; \
	(tx_socket_arg).block.len    = len_arg; \
	(tx_socket_arg).block.op     = VK_OP_WRITE; \
	(tx_socket_arg).block.copied = 0; \
	\
	(rx_socket_arg).block.buf    = NULL; \
	(rx_socket_arg).block.len    = len_arg; \
	(rx_socket_arg).block.op     = VK_OP_READ; \
	(rx_socket_arg).block.copied = 0; \
	while (!vk_vectoring_has_nodata(&(rx_socket_arg).rx.ring) && (tx_socket_arg).block.len > 0) { \
		(tx_socket_arg).block.rc = (rx_socket_arg).block.rc = vk_vectoring_recv_splice((rx_socket_arg)->rx.ring, (tx_socket_arg).tx.ring, (tx_socket_arg).block.len); \
		if ((tx_socket_arg).block.rc == -1) { \
			vk_error(); \
		} else { \
			(tx_socket_arg).block.len    -= (size_t) (tx_socket_arg).block.rc; \
			(tx_socket_arg).block.buf    += (size_t) (tx_socket_arg).block.rc; \
			(tx_socket_arg).block.copied += (size_t) (tx_socket_arg).block.rc; \
			\
			(rx_socket_arg).block.len    -= (size_t) (rx_socket_arg).block.rc; \
			(rx_socket_arg).block.buf    += (size_t) (rx_socket_arg).block.rc; \
			(rx_socket_arg).block.copied += (size_t) (rx_socket_arg).block.rc; \
		} \
		if (!vk_vectoring_has_nodata(&(rx_socket_arg).rx.ring) && (tx_socket_arg).block.len > 0) { \
			vk_wait(rx_socket_arg); \
		} \
		if ((tx_socket_arg).block.len > 0) { \
			vk_wait(tx_socket_arg); \
		} \
	} \
	rc_arg = (tx_socket_arg).block.copied; \
} while (0);

/* read from socket, splicing writes into the specified socket the specified length */
#define vk_socket_read_splice(rc_arg, rx_socket_arg, tx_socket_arg, len_arg) vk_socket_write_splice(rc_arg, tx_socket_arg, rx_socket_arg, len_arg)

/* above socket operations, but applying to the coroutine's standard socket */
#define vk_read(    rc_arg, buf_arg, len_arg) vk_socket_read(    rc_arg, *that->socket_ptr, buf_arg, len_arg)
#define vk_readline(rc_arg, buf_arg, len_arg) vk_socket_readline(rc_arg, *that->socket_ptr, buf_arg, len_arg)
#define vk_eof()                              vk_socket_eof(             *that->socket_ptr)
#define vk_clear()                            vk_socket_clear(           *that->socket_ptr)
#define vk_nodata()                           vk_socket_nodata(          *that->socket_ptr)
#define vk_hup()                              vk_socket_hup(             *that->socket_ptr)
#define vk_write(buf_arg, len_arg)            vk_socket_write(           *that->socket_ptr, buf_arg, len_arg)
#define vk_flush()                            vk_socket_flush(           *that->socket_ptr)
#define vk_read_splice( rc_arg, socket_arg, len_arg) vk_socket_read_splice( rc_arg, *that->socket_ptr, socket_arg, len_arg) 
#define vk_write_splice(rc_arg, socket_arg, len_arg) vk_socket_write_splice(rc_arg, *that->socket_ptr, socket_arg, len_arg) 

#endif
