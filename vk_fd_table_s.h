#ifndef VK_FD_TABLE_S_H
#define VK_FD_TABLE_S_H

#include "vk_fd_s.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#define VK_USE_KQUEUE
#elif __linux__
#define VK_USE_EPOLL
#endif

#if defined(VK_USE_KQUEUE)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#elif defined(VK_USE_EPOLL)
#include <sys/epoll.h>
#else
#include <poll.h>
#include <stdint.h>
#endif

#define VK_FD_MAX 16384
#define VK_EV_MAX 32

struct vk_fd_table {
	size_t size;
	SLIST_HEAD(dirty_fds_head, vk_fd) dirty_fds; /* head of list of FDs to register */
	SLIST_HEAD(fresh_fds_head, vk_fd) fresh_fds; /* head of list of FDs to dispatch */
#if defined(VK_USE_KQUEUE)
	int kq_initialized;
	struct kevent kq_changelist[VK_EV_MAX];
	int           kq_nchanges;
	struct kevent kq_eventlist[VK_EV_MAX];
	int           kq_nevents;
	int kq_fd;
#elif defined(VK_USE_EPOLL)
	int epoll_initialized;
	struct epoll_event epoll_events[VK_EV_MAX];
	int epoll_event_count;
	int epoll_fd;
#else
	struct pollfd poll_fds[VK_FD_MAX];
	nfds_t poll_nfds;
#endif
	struct vk_fd fds[];
};

#endif
