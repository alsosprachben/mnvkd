#ifndef VK_STATE_H
#define VK_STATE_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>

#include "vk_heap.h"
#include "vk_vectoring.h"

struct that;

struct that {
	void (*func)(struct that *that);
	char *file;
	int line;
	int counter;
	enum VK_PROC_STAT {
		VK_PROC_RUN = 0,
		VK_PROC_WAIT,
		VK_PROC_ERR,
		VK_PROC_END,
	} status;
	int error;
	struct vk_socket socket;
	struct vk_heap_descriptor hd;
	struct that *runq_prev;
	struct that *runq_next;
};
int vk_init(struct that *that, void (*func)(struct that *that), char *file, size_t line, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset);
int vk_deinit(struct that *that);
int vk_continue(struct that *that);
int vk_sync_unblock(struct that *that);

#define VK_INIT(that, vk_func,                           map_len) \
	vk_init(that, vk_func, __FILE__, __LINE__, NULL, map_len, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0)

#define VK_INIT_PRIVATE(that, vk_func,                   map_len) \
	vk_init(that, vk_func, __FILE__, __LINE__, NULL, map_len, PROT_NONE,            MAP_ANON, -1, 0)

#define vk_procdump(that, tag)      \
fprintf(                            \
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
	(that)->hd.addr_start,      \
	(that)->hd.addr_cursor,     \
	(that)->hd.addr_stop        \
)

/* allocation via the heap */

#define vk_calloc(val_ptr, nmemb) \
	(val_ptr) = vk_heap_push(&that->hd, (nmemb), sizeof (*(val_ptr))); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

#define vk_free() \
	rc = vk_heap_pop(&that->hd); \
	if (rc == -1) { \
		vk_error(); \
	}


/* state machine macros */

#define vk_begin()                    \
	self = that->hd.addr_start;   \
	that->file = __FILE__;        \
	that->status = VK_PROC_RUN;   \
	switch (that->counter) {      \
		case -1:              \
			vk_calloc(self, 1);


#define vk_end()                                    \
			vk_free();                  \
			that->status = VK_PROC_END; \
	}                                           \
	return

#define vk_yield(s) do {             \
	that->line = __LINE__;       \
	that->counter = __COUNTER__; \
	that->status = s;            \
	return;                      \
	case __COUNTER__ - 1:;       \
} while (0)                          \

#define vk_wait() vk_yield(VK_PROC_WAIT)

#define vk_raise(e) do {       \
	that->error = e;       \
	vk_yield(VK_PROC_ERR); \
} while (0)

#define vk_error() vk_raise(errno)

#define vk_socket_fd_read(rc, socket_arg, d) do { \
    rc = vk_vectoring_read(&(socket_arg).rx.ring, d); \
    vk_wait(); \
} while (0)

#define vk_socket_fd_write(rc, socket_arg, d) do { \
    rc = vk_vectoring_write(&(socket_arg).tx.ring, d); \
    vk_wait(0); \
} while (0)

#define vk_socket_read(socket_arg, buf_arg, len_arg) do { \
    (socket_arg).block.buf    = (buf_arg); \
    (socket_arg).block.len    = (len_arg); \
    (socket_arg).block.op     = VK_OP_READ; \
    (socket_arg).block.copied = 0; \
    while (!vk_vectoring_has_eof(&(socket_arg).rx.ring) && (socket_arg).block.len > 0) { \
        (socket_arg).block.rc = vk_vectoring_recv(&(socket_arg).rx.ring, (socket_arg).block.buf, (socket_arg).block.len); \
        if ((socket_arg).block.rc == -1) { \
            vk_error(); \
        } else if ((socket_arg).block.rc == 0) { \
        } else { \
            (socket_arg).block.len    -= (size_t) (socket_arg).block.rc; \
            (socket_arg).block.buf    += (size_t) (socket_arg).block.rc; \
            (socket_arg).block.copied += (size_t) (socket_arg).block.rc; \
        } \
        if (!vk_vectoring_has_eof(&(socket_arg).rx.ring) && (socket_arg).block.len > 0) { \
            vk_wait(); \
        } \
    } \
} while (0);

#define vk_socket_readline(rc_arg, socket_arg, buf_arg, len_arg) do { \
    (socket_arg).block.buf    = buf_arg; \
    (socket_arg).block.len    = len_arg; \
    (socket_arg).block.op     = VK_OP_READ; \
    (socket_arg).block.copied = 0; \
    while (!vk_vectoring_has_eof(&(socket_arg).rx.ring) && (socket_arg).block.len > 0 && ((socket_arg).block.copied == 0 || (socket_arg).block.buf[(socket_arg).block.copied - 1] != '\n')) { \
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
        if (!vk_vectoring_has_eof(&(socket_arg).rx.ring) && (socket_arg).block.len > 0 && ((socket_arg).block.copied == 0 || (socket_arg).block.buf[(socket_arg).block.copied - 1] != '\n')) { \
            vk_wait(); \
        } \
        rc_arg = (socket_arg).block.copied; \
    } \
} while (0);

#define vk_socket_eof(socket_arg) vk_vectoring_has_eof(&(socket_arg).rx.ring)


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
            vk_wait(); \
        } \
    } \
} while (0);

#define vk_socket_flush(socket_arg) do { \
    (socket_arg).block.buf = NULL; \
    (socket_arg).block.len = 0; \
    (socket_arg).block.op  = VK_OP_WRITE; \
    (socket_arg).block.rc  = 0; \
    while (vk_vectoring_tx_len(&(socket_arg).tx.ring) > 0) { \
        vk_wait(); \
    } \
} while (0);

#define vk_read(buf_arg, len_arg)             vk_socket_read(            that->socket, buf_arg, len_arg)
#define vk_readline(rc_arg, buf_arg, len_arg) vk_socket_readline(rc_arg, that->socket, buf_arg, len_arg)
#define vk_eof()                              vk_socket_eof(             that->socket)
#define vk_write(buf_arg, len_arg)            vk_socket_write(           that->socket, buf_arg, len_arg)
#define vk_flush()                            vk_socket_flush(           that->socket)

#endif
