#ifndef VK_STATE_S_H
#define VK_STATE_S_H

#include <sys/queue.h>

#include "vk_proc.h"
#include "vk_socket.h"
#include "vk_pipe.h"
#include "vk_pipe_s.h"
#include "vk_state.h"

/* The coroutine process struct. The coroutine function's state is the pointer `self` to its heap. */
struct that {
	void (*func)(struct that *that);
	const char *func_name;
	char *file;
	int line;
	int counter;
	enum VK_PROC_STAT status;
	int error;
	int error_counter;
	struct vk_proc *proc_ptr;
	void *self;
	struct vk_socket *socket_ptr;
	struct vk_socket *waiting_socket_ptr;
	struct future *ft_ptr;
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	SLIST_ENTRY(that) run_q_elem;
	int run_enq;
};

#endif