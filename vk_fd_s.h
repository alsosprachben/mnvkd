#ifndef VK_FD_S_H
#define VK_FD_S_H

#include "vk_queue.h"

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
	int allocated;
	int dirty; /* to register */
	int fresh; /* to dispatch */
	SLIST_ENTRY(vk_fd) dirty_elem; /* element in dirty list */
	SLIST_ENTRY(vk_fd) fresh_elem; /* element in fresh list */
	struct vk_io_future ioft_post; /* state registered or polled:    physical, posterior */
	struct vk_io_future ioft_pre;  /* state to register or dispatch: logical,  prior */
};

struct vk_fd_table {
	size_t size;
	SLIST_HEAD(dirty_fds_head, vk_fd) dirty_fds; /* head of list of FDs to register */
	SLIST_HEAD(fresh_fds_head, vk_fd) fresh_fds; /* head of list of FDs to dispatch */
	struct vk_fd fds[];
};

#endif
