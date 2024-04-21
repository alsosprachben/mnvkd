#ifndef VK_IO_FUTURE_S_H
#define VK_IO_FUTURE_S_H

#include <poll.h>
#include <stdint.h>
#include <unistd.h>

struct vk_socket;

/*
 * `struct vk_io_future` encapsulates:
 *  - `struct pollfd` and/or `struct kevent`, associating with the blocked coroutine and its heap, for future execution.
 *  - the 1:1 relationship between `struct vk_socket` and `struct vk_fd`
 *    via `socket_ptr` and `event.fd` in `vk_fd_table_get()`.
 */
struct vk_io_future {
	struct vk_socket* socket_ptr; /* blocked socket */
	struct pollfd event;	      /* poller event */
	size_t readable;	      /* number of bytes available to be read */
	size_t writable;	      /* number of bytes available to be written */
	int rx_closed;		      /* signal to close the read  side in the poller */
	int tx_closed;		      /* signal to close the write side in the poller */
};

#endif
