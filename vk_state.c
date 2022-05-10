#include "vk_state.h"

#include "vk_heap.h"
#include "debug.h"

void vk_init(struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), struct vk_pipe rx_fd, struct vk_pipe tx_fd, const char *func_name, char *file, size_t line) {
	that->func = func;
	that->func_name = func_name;
	that->file = file;
	that->line = line;
	that->counter = -1;
	that->status = VK_PROC_RUN;
	that->error = 0;
	that->error_counter = -2;
	that->rx_fd = rx_fd;
	that->tx_fd = tx_fd;
	that->socket_ptr = NULL;
	that->proc_ptr = proc_ptr;
	that->self = vk_heap_get_cursor(vk_proc_get_heap(vk_get_proc(that)));
	that->ft_ptr = NULL;
	that->run_q_elem.sle_next = NULL;
	that->run_enq = 0;
	that->blocked_q_elem.sle_next = NULL;
	that->blocked_enq = 0;
}

void vk_init_fds(struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), int rx_fd_arg, int tx_fd_arg, const char *func_name, char *file, size_t line) {
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	VK_PIPE_INIT_FD(rx_fd, rx_fd_arg);
	VK_PIPE_INIT_FD(tx_fd, tx_fd_arg);
	return vk_init(that, proc_ptr, func, rx_fd, tx_fd, func_name, file, line);
}

void vk_init_child(struct that *parent, struct that *that, void (*func)(struct that *that), const char *func_name, char *file, size_t line) {
	//struct vk_pipe rx_fd;
	//struct vk_pipe tx_fd;
	//VK_PIPE_INIT_TX(rx_fd, *(parent)->socket_ptr);                       /* child  read  of       parent write    */
	//VK_PIPE_INIT_FD(tx_fd, VK_PIPE_GET_FD((parent)->socket_ptr->tx_fd)); /* child  write of prior parent write FD */
	//VK_PIPE_INIT_RX((parent)->socket_ptr->tx_fd, *(that)->socket_ptr);   /* parent write of       child  read     */
	                                                                     /* parent read  remains  parent read FD  */
	return vk_init(that, parent->proc_ptr, func, parent->rx_fd, parent->tx_fd, func_name, file, line);																 
}

int vk_deinit(struct that *that) {
	return 0;
}

struct vk_proc *vk_get_proc(struct that *that) {
	return that->proc_ptr;
}

void vk_enqueue_run(struct that *that) {
	vk_proc_enqueue_run(that->proc_ptr, that);
}

void vk_enqueue_blocked(struct that *that) {
	vk_proc_enqueue_blocked(that->proc_ptr, that);
}

int vk_is_completed(struct that *that) {
	return that->status == VK_PROC_END;
}

int vk_is_ready(struct that *that) {
	return that->status == VK_PROC_RUN || that->status == VK_PROC_ERR;
}

int vk_is_yielding(struct that *that) {
	return that->status == VK_PROC_YIELD;
}

/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct that *that) {
	DBG("->vk_ready("PRIvk", NULL): setting to RUN\n", ARGvk(that));
	that->status = VK_PROC_RUN;
}

ssize_t vk_unblock(struct that *that) {
	ssize_t rc;
	switch (that->status) {
		case VK_PROC_WAIT:
			if (that->waiting_socket_ptr != NULL) {
				/*
				vk_vectoring_printf(&that->waiting_socket_ptr->rx.ring, "pre-rx");
				vk_vectoring_printf(&that->waiting_socket_ptr->tx.ring, "pre-tx");
				*/

				rc = vk_socket_handler(that->waiting_socket_ptr);
				if (rc == -1) {
					return -1;
				}

				/*
				vk_vectoring_printf(&that->waiting_socket_ptr->rx.ring, "post-rx");
				vk_vectoring_printf(&that->waiting_socket_ptr->tx.ring, "post-tx");
				*/
				return rc;
			} else {
				errno = EINVAL;
				return -1;
			}
			break;
		default:
			break;
	}
	return 0;
}

