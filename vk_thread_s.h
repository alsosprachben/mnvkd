#ifndef VK_STATE_S_H
#define VK_STATE_S_H

#include "vk_queue.h"
#include "vk_pipe_s.h"

struct vk_socket;

/* The coroutine process struct. The coroutine function's state is the pointer `self` to its heap. */
struct vk_thread {
	/* void (*func)(struct vk_thread *that); */
	vk_func func;

	/* debug info */
	const char *func_name;
	char *file;
	int line;

	/* coroutine state */
	int counter;
	enum VK_PROC_STAT status;
	int error; /* `errno` via `vk_raise()` */
	int error_counter; /* where to `vk_lower()` back to */
	struct vk_proc_local *proc_local_ptr;
	void *self;

	/* standard I/O */
	struct vk_socket *socket_ptr; /* the default socket for standard I/O (like FD 0 and 1 in Unix) */
	struct vk_socket *waiting_socket_ptr; /* the socket that the coroutine is waiting on, or NULL if not */
	TAILQ_HEAD(ft_q_head, vk_future) ft_q; /* future queue for vk_listen() */
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;

	/* distinct thread run queue for local process -- head on `struct vk_proc_local` at `run_q` */
	SLIST_ENTRY(vk_thread) run_q_elem;
	int run_enq; /* to make entries distinct */
};

#endif