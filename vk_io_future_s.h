#ifndef VK_IO_FUTURE_S_H
#define VK_IO_FUTURE_S_H

#include <poll.h>
#include <stdint.h>

struct vk_socket;

/* `struct vk_io_future` encapsulates `struct pollfd` and/or `struct kevent`, associating with the blocked coroutine and its heap, for future execution. */
struct vk_io_future {
	struct vk_socket *socket_ptr; /* blocked socket */
	struct pollfd event;          /* poller event */
	intptr_t data;                /* kevent data */
	int closed;                   /* signal that the FD is closed, so update our representation in the poller */
};

#endif