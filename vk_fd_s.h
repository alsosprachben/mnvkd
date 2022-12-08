#ifndef VK_FD_S_H
#define VK_FD_S_H

#include <poll.h>
#include <stdint.h>

/* `struct vk_io_future` encapsulates `struct pollfd` and/or `struct kevent`, associating with the blocked coroutine and its heap, for future execution. */
struct vk_io_future {
	struct vk_socket *socket_ptr;  /* blocked socket */
	struct pollfd event;      /* poller event */
	intptr_t data;            /* kevent data */
};


struct vk_fd {
	int fd;
	size_t proc_id;
	struct vk_io_future ioft_post; /* physical, posterior, registered to the system */
	struct vk_io_future ioft_pre;  /* logical,  prior,     pending locally */
};

struct vk_fd_table {
	size_t size;
	struct vk_fd fds[];
};

#endif
