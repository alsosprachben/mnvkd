#ifndef VK_STATE_H
#define VK_STATE_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>

#include "vk_heap.h"
#include "vk_socket.h"

struct that;
struct future;

struct future {
	struct future *next; /* for reentrance */
	struct that *vk;
	intptr_t msg;
	int error; /* errno */
};

#define future_get(ft)     ((void *) (ft).msg)
#define future_bind(ft, vk_ptr)     ((ft).vk = (vk_ptr)
#define future_resolve(ft, msg_arg) ((ft).msg = (msg_arg))

struct that {
	void (*func)(struct that *that);
	const char *func_name;
	char *file;
	int line;
	int counter;
	enum VK_PROC_STAT {
		VK_PROC_RUN = 0,
		VK_PROC_LISTEN,
		VK_PROC_WAIT,
		VK_PROC_ERR,
		VK_PROC_END,
	} status;
	int error;
	int error_counter;
	struct vk_heap_descriptor hd;
	struct vk_heap_descriptor *hd_ptr;
	struct vk_socket socket;
	struct vk_socket *waiting_socket_ptr;
	ssize_t (*unblocker)(struct that *that);
	void *self;
	struct future *ft_ptr;
	struct that *run_next;
};
int vk_init(struct that *that, void (*func)(struct that *that), ssize_t (*unblocker)(struct that *that), struct vk_pipe rx_fd, struct vk_pipe tx_fd, const char *func_name, char *file, size_t line, struct vk_heap_descriptor *hd_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset);
int vk_deinit(struct that *that);
int vk_execute(struct that *that);
int vk_completed(struct that *that);
void vk_enqueue(struct that *that, struct that *there);
/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct that *that);
ssize_t vk_sync_unblock(struct that *that);

/* primary coroutine with public memory */
#define VK_INIT(        rc_arg, that, vk_func, unblocker, rx_fd_arg, tx_fd_arg,                                                map_len) { \
	struct vk_pipe __vk_rx_fd; \
	struct vk_pipe __vk_tx_fd; \
	VK_PIPE_INIT_FD(__vk_rx_fd, rx_fd_arg); \
	VK_PIPE_INIT_FD(__vk_tx_fd, tx_fd_arg); \
	rc_arg = vk_init(        that, vk_func, unblocker, __vk_rx_fd, __vk_tx_fd, #vk_func, __FILE__, __LINE__, NULL,             NULL, map_len, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0); \
}

/* primary coroutine with private memory */
#define VK_INIT_PRIVATE(rc_arg, that, vk_func, unblocker, rx_fd_arg, tx_fd_arg,                                                map_len) { \
	struct vk_pipe __vk_rx_fd; \
	struct vk_pipe __vk_tx_fd; \
	VK_PIPE_INIT_FD(__vk_rx_fd, rx_fd_arg); \
	VK_PIPE_INIT_FD(__vk_tx_fd, tx_fd_arg); \
	rc_arg = vk_init(        that, vk_func, unblocker, __vk_rx_fd, __vk_tx_fd, #vk_func, __FILE__, __LINE__, NULL,             NULL, map_len, PROT_NONE,            MAP_ANON|MAP_PRIVATE, -1, 0); \
}

/* child coroutine that takes over writes from the parent, connecting via internal pipe the parent's writes to the child's reads */
/* lifecycle: parent read FD -> parent write pipe -> child read pipe -> child write FD */
/* This allows the responder to start writing before the reading is complete. */
#define VK_INIT_RESPONDER(rc_arg, parent, that, vk_func, unblocker,                                                            map_len) { \
	struct vk_pipe __vk_rx_fd; \
	struct vk_pipe __vk_tx_fd; \
	VK_PIPE_INIT_TX(__vk_rx_fd, (parent)->socket);                       /* child  read  of       parent write    */ \
	VK_PIPE_INIT_FD(__vk_tx_fd, VK_PIPE_GET_FD((parent)->socket.tx_fd)); /* child  write of prior parent write FD */ \
	VK_PIPE_INIT_RX((parent)->socket.tx_fd, (that)->socket);             /* parent write of       child  read     */ \
	                                                                     /* parent read  remains  parent read FD  */ \
	rc_arg = vk_init(        that, vk_func, unblocker, __vk_rx_fd, __vk_tx_fd, #vk_func, __FILE__, __LINE__, (parent)->hd_ptr, NULL, map_len, 0,                    0,         0, 0); \
}
/* coroutine-scoped for responder */
#define vk_responder(rc_arg, child, vk_func, map_len) VK_INIT_RESPONDER(rc_arg, that, child, vk_func, that->unblocker, map_len)

/* coroutine-scoped for accepted socket into new heap */
#define vk_accepted(rc_arg, parent, vk_func, rx_fd_arg, tx_fd_arg, map_len) VK_INIT_PRIVATE(rc_arg, parent, vk_func, that->unblocker, rx_fd_arg, tx_fd_arg, map_len)

#define vk_procdump(that, tag)                      \
	fprintf(                                    \
			stderr,                     \
			"tag: %s\n"                 \
			"line: %s:%i\n"             \
			"counter: %i\n"             \
			"status: %i\n"              \
			"error: %i\n"               \
			"msg: %p\n"                 \
			"start: %p\n"               \
			"cursor: %p\n"              \
			"stop: %p\n"                \
			"\n"                        \
			,                           \
			(tag),                      \
			(that)->file, (that)->line, \
			(that)->counter,            \
			(that)->status,             \
			(that)->error,              \
			(void *) (that)->msg,       \
			(that)->hd_ptr->addr_start,      \
			(that)->hd_ptr->addr_cursor,     \
			(that)->hd_ptr->addr_stop        \
	)

/* allocation via the heap */

/* allocate an array of the pointer from the micro-heap as a stack */
#define vk_calloc(val_ptr, nmemb) \
	(val_ptr) = vk_heap_push(that->hd_ptr, (nmemb), sizeof (*(val_ptr))); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

/* de-allocate the last array from the micro-heap stack */
#define vk_free()                     \
	rc = vk_heap_pop(&that->hd);  \
	if (rc == -1) {               \
		vk_perror("vk_free"); \
	}


/* state machine macros */

/* continue RUN state, branching to blocked entrypoint, or error entrypoint, and allocate self */
#define vk_begin()                     \
self = that->self;                     \
if (that->status == VK_PROC_ERR) {     \
	that->counter = -2;            \
}                                      \
switch (that->counter) {               \
	case -1:                       \
		that->file = __FILE__; \
		that->func_name = __func__; \
		vk_calloc(self, 1);         \
		that->self = self;

/* de-allocate self and set END state */
#define vk_end()                            \
	default:                            \
		vk_free();                  \
		that->status = VK_PROC_END; \
}                                           \
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
#define vk_play(there) vk_enqueue(that, there)

/* stop coroutine in RUN state */
#define vk_pause() do {        \
	vk_yield(VK_PROC_RUN); \
} while (0)

/* schedule the callee to run, then stop the coroutine in RUN state */
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
} while (0)

/* pause coroutine, receiving message on play */
#define vk_listen(recv_ft) do {            \
	vk_yield(VK_PROC_LISTEN);          \
	(recv_ft).vk  = that->ft_ptr->vk;  \
	(recv_ft).msg = that->ft_ptr->msg; \
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

/* splice into socket from the specified socket the specified length */
#define vk_socket_tee(rc_arg, socket_arg, other_arg, len_arg) do { \
	(socket_arg).block.buf    = NULL; \
	(socket_arg).block.len    = len_arg; \
	(socket_arg).block.op     = VK_OP_WRITE; \
	(socket_arg).block.copied = 0; \
	while (!vk_vectoring_has_eof(&(socket_arg).rx.ring) && (socket_arg).block.len > 0) { \
		(socket_arg).block.rc = vk_vectoring_recv_splice((other_arg)->rx.ring, (socket_arg).tx.ring, (socket_arg).block.len); \
		if ((socket_arg).block.rc == -1) { \
			vk_error(); \
		} else { \
			(socket_arg).block.len    -= (size_t) (socket_arg).block.rc; \
			(socket_arg).block.buf    += (size_t) (socket_arg).block.rc; \
			(socket_arg).block.copied += (size_t) (socket_arg).block.rc; \
		} \
		if (!vk_vectoring_has_nodata(&(other_arg).rx.ring) && (other_arg).block.len > 0) { \
			(t_arg).block.op = VK_OP_READ; \
			vk_wait(other_arg); \
		} \
		if ((socket_arg).block.len > 0) { \
			(t_arg).block.op = VK_OP_WRITE; \
			vk_wait(socket_arg); \
		} \
	} \
	rc_arg = (socket_arg).block.copied; \
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

/* above socket operations, but applying to the coroutine's standard socket */
#define vk_read(    rc_arg, buf_arg, len_arg) vk_socket_read(    rc_arg, that->socket, buf_arg, len_arg)
#define vk_readline(rc_arg, buf_arg, len_arg) vk_socket_readline(rc_arg, that->socket, buf_arg, len_arg)
#define vk_eof()                              vk_socket_eof(             that->socket)
#define vk_clear()                            vk_socket_clear(           that->socket)
#define vk_nodata()                           vk_socket_nodata(          that->socket)
#define vk_hup()                              vk_socket_hup(             that->socket)
#define vk_write(buf_arg, len_arg)            vk_socket_write(           that->socket, buf_arg, len_arg)
#define vk_flush()                            vk_socket_flush(           that->socket)

#endif
