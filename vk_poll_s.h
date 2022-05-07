#include "vk_poll.h"

#include <poll.h>
#include <stdint.h>

/* `struct io_future` encapsulates `struct pollfd` and/or `struct kevent`, associating with the blocked coroutine and its heap, for future execution. */
struct io_future {
	struct vk_proc *proc_ptr; /* for entering the process heap */
	struct that *blocked_vk;  /* blocked coroutine */
	struct pollfd event;      /* poller event */
	intptr_t data;            /* kevent data */
};