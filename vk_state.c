#include "vk_state.h"

#include "vk_heap.h"
#include "debug.h"

int vk_init(struct that *that, void (*func)(struct that *that), ssize_t (*unblocker)(struct that *that), struct vk_pipe rx_fd, struct vk_pipe tx_fd, const char *func_name, char *file, size_t line, struct vk_heap_descriptor *hd_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset) {
	int rc;

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
	if (hd_ptr != NULL) {
		that->hd_ptr = hd_ptr;
		rc = 0;
	} else {
		that->hd_ptr = &that->hd;
		rc = vk_heap_map(that->hd_ptr, map_addr, map_len, map_prot, map_flags, map_fd, map_offset);
		if (rc == -1) {
			return -1;
		}
	}
	that->self = that->hd_ptr->addr_start;

	return rc;
}
int vk_deinit(struct that *that) {
	return vk_heap_unmap(that->hd_ptr);
}
int vk_execute(struct that *that, struct that *sub) {
	int rc;
	int effect;
	struct that *that2; /* cursor for per-coroutine run-q iteration */
	struct that *that3; /* for clearing the run_next member of that2 after setting that2 as the next iteration */
	DBG("--vk_execute("PRIvk")\n", ARGvk(that));

	while (that->status == VK_PROC_RUN || that->status == VK_PROC_ERR) {
		rc = vk_heap_enter(that->hd_ptr);
		if (rc == -1) {
			return -1;
		}

		that2 = sub == NULL ? that : sub;
		do {
			do {
				DBG("  EXEC@"PRIvk"\n", ARGvk(that2));
				that2->func(that2);
				DBG("  STOP@"PRIvk"\n", ARGvk(that2));
				effect = that2->unblocker(that2);
				if (effect == -1) {
					return -1;
				}
				if (effect) {
					vk_ready(that2);
				}
			} while (that2->status == VK_PROC_RUN || that2->status == VK_PROC_ERR);

			if (that2->status == VK_PROC_YIELD) {
				/* 
				 * Yielded coroutines are already added to the end of the run queue,
				 * but are left in yield state to break out of the preceeding loop,
				 * and need to be set back to run state once past the preceeding loop.
				 */
				DBG(" YIELD@"PRIvk"\n", ARGvk(that2));
				that2->status = VK_PROC_RUN;
			} 

			/* op is blocked, run next in queue */
			that3 = that2;
			that2 = that2->run_next;
			that3->run_next = NULL;
			if (that2 != NULL) {
				DBG("<-vk_dequeue("PRIvk", "PRIvk"): dequeing\n", ARGvk(that), ARGvk(that2));
			}
		} while (that2 != NULL);

		rc = vk_heap_exit(that->hd_ptr);
		if (rc == -1) {
			return -1;
		}
	}
	return 0;
}

int vk_completed(struct that *that) {
	return that->status == VK_PROC_END;
}

void vk_enqueue(struct that *that, struct that *there) {
	struct that *vk_cursor;
	if (there == NULL) {
		DBG("->vk_enqueue("PRIvk", NULL): NOT enqueuing\n", ARGvk(that));
		return;
	}
	vk_ready(there);
	if (that == there) {
		DBG("->vk_enqueue("PRIvk", ==): enqueuing self\n", ARGvk(that));
		return;
	}

	vk_cursor = that;
	DBG("->vk_enqueue("PRIvk", "PRIvk"): enqueuing...\n", ARGvk(that), ARGvk(there));
	while (vk_cursor->run_next != NULL) {
		DBG("  "PRIvk" -> "PRIvk"\n", ARGvk(vk_cursor), ARGvk(vk_cursor->run_next));
		vk_cursor = vk_cursor->run_next;
		if (vk_cursor == there) {
			DBG("->vk_enqueue("PRIvk", "PRIvk"): already enqueued\n", ARGvk(that), ARGvk(there));
			return; /* already enqueued */
		}
	}
	vk_cursor->run_next = there;
	DBG("->vk_enqueue("PRIvk", "PRIvk"): enqueued\n", ARGvk(that), ARGvk(there));
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

