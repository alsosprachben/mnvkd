#ifndef VK_POLL_H
#define VK_POLL_H
#include <poll.h>
#include <stdint.h>
#include "vk_poll.h"

struct that;

/* `struct io_future` encapsulates `struct pollfd` and/or `struct kevent`, associating with the blocked coroutine and its heap, for future execution. */
struct io_future {
	struct vk_heap_descriptor *heap; /* for entering the heap */
	struct that *blocked_vk;         /* blocked coroutine */
	struct pollfd event;             /* poller event */
	intptr_t data;                   /* kevent data */
};

void io_future_init(struct io_future *ioft, struct that *blocked_vk);

#endif
