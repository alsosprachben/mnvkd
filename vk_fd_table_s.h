#ifndef VK_FD_TABLE_S_H
#define VK_FD_TABLE_S_H

#ifndef VK_USE_KQUEUE
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#define VK_USE_KQUEUE 1
#else
#define VK_USE_KQUEUE 0
#endif
#endif

#ifndef VK_USE_EPOLL
#if defined(__linux__)
#define VK_USE_EPOLL 1
#else
#define VK_USE_EPOLL 0
#endif
#endif

#ifndef VK_USE_GETEVENTS
#if defined(__linux__)
#define VK_USE_GETEVENTS 1
#else
#define VK_USE_GETEVENTS 0
#endif
#endif

#include "vk_fd_s.h"
#include "vk_sys_caps_s.h"

#if VK_USE_KQUEUE
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#if VK_USE_EPOLL
#include <sys/epoll.h>
#endif

#if VK_USE_GETEVENTS
#include <linux/aio_abi.h>
#include <sys/syscall.h>
#endif

#if !VK_USE_KQUEUE && !VK_USE_EPOLL && !VK_USE_GETEVENTS
#include <poll.h>
#include <stdint.h>
#endif

#define VK_FD_MAX 16384
#define VK_EV_MAX 32

#if VK_USE_KQUEUE || VK_USE_EPOLL || VK_USE_GETEVENTS
enum vk_os_driver_kind {
	VK_OS_DRIVER_NONE = 0,
	VK_OS_DRIVER_KQUEUE,
	VK_OS_DRIVER_EPOLL,
	VK_OS_DRIVER_GETEVENTS,
};
#endif

struct vk_fd_table {
	/* FD Table Header */
	size_t size;				     /* size of the `fds` record array */
	SLIST_HEAD(dirty_fds_head, vk_fd) dirty_fds; /* head of list of FDs to register */
	SLIST_HEAD(fresh_fds_head, vk_fd) fresh_fds; /* head of list of FDs to dispatch */
	enum vk_poll_driver poll_driver;
	enum vk_poll_driver poll_driver_requested;
	enum vk_poll_method poll_method;
	enum vk_os_driver_kind os_driver_kind;
	struct vk_sys_caps sys_caps;
	int sys_caps_initialized;

	/* per-poller driver state */
#if VK_USE_KQUEUE
	int kq_initialized;
	struct kevent kq_changelist[VK_EV_MAX];
	int kq_nchanges;
	struct kevent kq_eventlist[VK_EV_MAX];
	int kq_nevents;
	int kq_fd;
#endif

#if VK_USE_EPOLL
	int epoll_initialized;
	struct epoll_event epoll_events[VK_EV_MAX];
	int epoll_event_count;
	int epoll_fd;
#endif

#if VK_USE_GETEVENTS
	int aio_initialized;
	aio_context_t aio_ctx; // AIO context
#endif

	/* poll array, a logical state for poll(), used by various pollers */
	struct pollfd poll_fds[VK_FD_MAX];
	nfds_t poll_nfds;

	/* FD Table Body */
	struct vk_fd fds[]; /* array size is the `size` property */
};

#endif
