#ifndef VK_FD_TABLE_S_H
#define VK_FD_TABLE_S_H

#include "vk_fd_s.h"

#include <poll.h>
#include <stdint.h>

#ifdef __linux__
#include <sys/epoll.h>
#endif

#define VK_FD_MAX 16384
#define VK_EV_MAX 32

struct vk_fd_table {
	size_t size;
	SLIST_HEAD(dirty_fds_head, vk_fd) dirty_fds; /* head of list of FDs to register */
	SLIST_HEAD(fresh_fds_head, vk_fd) fresh_fds; /* head of list of FDs to dispatch */
	struct pollfd poll_fds[VK_FD_MAX];
	nfds_t poll_nfds;
#ifdef __linux__
	int epoll_initialized;
	struct epoll_event epoll_events[VK_EV_MAX];
	int epoll_event_count;
	int epoll_fd;
#endif
	struct vk_fd fds[];
};

#endif