#ifndef VK_STATE_H
#define VK_STATE_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>

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
	intptr_t msg;
	void *stack;
};

#define vk_init(that, vk_func) {   \
	(that)->func = (vk_func);  \
	(that)->file = __FILE__;   \
	(that)->line = __LINE__;   \
	(that)->counter = -1;      \
	(that)->status = 0;        \
	(that)->error = 0;         \
	(that)->msg = 0;           \
	(that)->stack = (void *)0; \
}

#define vk_procdump(that, tag)      \
fprintf(                            \
	stderr,                     \
	"tag: %s\n"                 \
	"line: %s:%i\n"             \
	"counter: %i\n"             \
	"status: %i\n"              \
	"error: %i\n"               \
	"msg: %p\n"                 \
	"stack: %p\n"               \
	"\n"                        \
	,                           \
	(tag),                      \
	(that)->file, (that)->line, \
	(that)->counter,            \
	(that)->status,             \
	(that)->error,              \
	(void *) (that)->msg,       \
	(that)->stack               \
)

#define vk_proc(name) void name(struct *that)

#define vk_begin()                    \
	that->file = __FILE__;        \
	that->status = VK_PROC_RUN; \
	switch (that->counter) {      \
		case -1:              \


#define vk_end() }                  \
	that->status = VK_PROC_END; \
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

#define vk_msg(m) do { \
	that->msg = m; \
	vk_wait();     \
} while (0)

#endif
