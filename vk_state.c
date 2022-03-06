#include "vk_state.h"

#include "vk_heap.h"

int vk_init(struct that *that, void (*func)(struct that *that), struct vk_pipe rx_fd, struct vk_pipe tx_fd, char *file, size_t line, struct vk_heap_descriptor *hd_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset) {
	int rc;

	that->func = func;
	that->file = file;
	that->line = line;
	that->counter = -1;
	that->status = 0;
	that->error = 0;
	VK_SOCKET_INIT(that->socket, rx_fd, tx_fd);
	if (hd_ptr != NULL) {
		that->hd_ptr = hd_ptr;
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
int vk_execute(struct that *that) {
	int rc;
	struct that *that2;
	struct that *that3;

	while (that->status == VK_PROC_RUN || that->status == VK_PROC_ERR) {
		rc = vk_heap_enter(that->hd_ptr);
		if (rc == -1) {
			return -1;
		}

		that2 = that;
		do {
			do {
				that2->status = VK_PROC_RUN;
				that2->func(that2);
				rc = vk_sync_unblock(that2);
				if (rc == -1) {
					return -1;
				}
			} while (that2->status == VK_PROC_WAIT);
			that3 = that2;
			that2 = that2->run_next;
			that3->run_next = NULL;
		} while (that2 != NULL);

		rc = vk_heap_exit(that->hd_ptr);
		if (rc == -1) {
			return -1;
		}
	}
	return 0;
}

int vk_run(struct that *that) {
	that->status = VK_PROC_RUN;
	return vk_execute(that);
}

int vk_runnable(struct that *that) {
	return that->status != VK_PROC_END && that->status != VK_PROC_ERR;
}

int vk_sync_unblock(struct that *that) {
	switch (that->status) {
		case VK_PROC_WAIT:
			if (that->waiting_socket_ptr != NULL) {
				ssize_t rc;

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

