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
	//vk_socket_init(that->socket_ptr, that, rx_fd, tx_fd);
	that->proc_ptr = proc_ptr;
	that->self = vk_heap_get_cursor(vk_proc_get_heap(vk_get_proc(that)));
	that->ft_ptr = NULL;
	that->run_q_elem.sle_next = NULL;
	that->blocked_q_elem.sle_next = NULL;
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

