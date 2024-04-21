#ifndef VK_FD_E_H
#define VK_FD_E_H

/*
 * Declare enums for `struct vk_fd`.
 *
 * Enums cannot be forward-declared anymore, because they may be smaller than an int depending on the number of values.
 */

enum vk_fd_type {
	VK_FD_TYPE_UNKNOWN = 0,
	/* POSIX */
	VK_FD_TYPE_PIPE,
	VK_FD_TYPE_SOCKET_STREAM,
	VK_FD_TYPE_SOCKET_DATAGRAM,
	VK_FD_TYPE_SOCKET_LISTEN, /* accept() as a read() operation */
	VK_FD_TYPE_FILE,
	VK_FD_TYPE_FILE_HEAP, /* use `struct vk_heap` to mmap-ahead file */
	/* BSD */
	VK_FD_TYPE_KQUEUE, /* kqueue() */
	/* Linux */
	VK_FD_TYPE_EPOLL,   /* epoll_create() */
	VK_FD_TYPE_INOTIFY, /* inotify_init() */
	VK_FD_TYPE_EVENT,   /* eventfd() */
	VK_FD_TYPE_TIMER,   /* timerfd_create() */
	VK_FD_TYPE_SIGNAL,  /* signalfd() */
};

#endif