#include "vk_state.h"

#include "vk_heap.h"
#include "vk_proc_s.h"
#include "debug.h"

void vk_init(struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), ssize_t (*unblocker)(struct that *that), struct vk_pipe rx_fd, struct vk_pipe tx_fd, const char *func_name, char *file, size_t line) {
	that->func = func;
	that->func_name = func_name;
	that->file = file;
	that->line = line;
	that->counter = -1;
	that->status = VK_PROC_RUN;
	that->error = 0;
	that->error_counter = -2;
	VK_SOCKET_INIT(that->socket, that, rx_fd, tx_fd);
	that->unblocker = unblocker;
	that->proc_ptr = proc_ptr;
	that->hd_ptr = &proc_ptr->heap;
	that->self = that->hd_ptr->addr_start;
	that->ft_ptr = NULL;
	that->run_q_elem.sle_next = NULL;
	that->blocked_q_elem.sle_next = NULL;
}
int vk_deinit(struct that *that) {
	return 0;
}
int vk_execute(struct vk_proc *proc_ptr, struct that *that) {
	int rc;
	DBG("--vk_execute("PRIvk")\n", ARGvk(that));

	rc = vk_heap_enter(&proc_ptr->heap);
	if (rc == -1) {
		return -1;
	}
	while (that->status == VK_PROC_RUN || that->status == VK_PROC_ERR) {
		do {
			do {
				DBG("  EXEC@"PRIvk"\n", ARGvk(that));
				that->func(that);
				DBG("  STOP@"PRIvk"\n", ARGvk(that));
				rc = that->unblocker(that);
				if (rc == -1) {
					return -1;
				}
			} while (that->status == VK_PROC_RUN || that->status == VK_PROC_ERR);

			if (that->status == VK_PROC_YIELD) {
				/* 
				 * Yielded coroutines are already added to the end of the run queue,
				 * but are left in yield state to break out of the preceeding loop,
				 * and need to be set back to run state once past the preceeding loop.
				 */
				DBG(" YIELD@"PRIvk"\n", ARGvk(that));
				that->status = VK_PROC_RUN;
			} 

			/* op is blocked, run next in queue */
			that = SLIST_FIRST(&proc_ptr->run_q);
			if (that != NULL) {
				SLIST_REMOVE_HEAD(&proc_ptr->run_q, run_q_elem);
			}
		} while (that != NULL);

	}

	rc = vk_heap_exit(&proc_ptr->heap);
	if (rc == -1) {
		return -1;
	}

	return 0;
}

int vk_completed(struct that *that) {
	return that->status == VK_PROC_END;
}

/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct that *that) {
	DBG("->vk_ready("PRIvk", NULL): setting to RUN\n", ARGvk(that));
	that->status = VK_PROC_RUN;
}

ssize_t vk_sync_unblock(struct that *that) {
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

