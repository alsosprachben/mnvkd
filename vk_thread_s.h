#ifndef VK_STATE_S_H
#define VK_STATE_S_H

#include "vk_queue.h"
#include "vk_proc.h"
#include "vk_socket.h"
#include "vk_pipe.h"
#include "vk_pipe_s.h"
#include "vk_thread.h"

/* The coroutine process struct. The coroutine function's state is the pointer `self` to its heap. */
struct vk_thread {
	/* void (*func)(struct vk_thread *that); */
	vk_func func;
	const char *func_name;
	char *file;
	int line;
	int counter;
	enum VK_PROC_STAT status;
	int error;
	int error_counter;
	struct vk_proc_local *proc_local_ptr;
	void *self;
	struct vk_socket *socket_ptr;
	struct vk_socket *waiting_socket_ptr;
	TAILQ_HEAD(ft_q_head, vk_future) ft_q;
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	SLIST_ENTRY(vk_thread) run_q_elem;
	int run_enq;
};

#endif