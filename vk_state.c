#include "vk_state.h"

#include "vk_heap.h"

int vk_init(struct that *that, void (*func)(struct that *that), int rx_fd, int tx_fd, char *file, size_t line, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset) {
	that->func = func;
	that->file = file;
	that->line = line;
	that->counter = -1;
	that->status = 0;
	that->error = 0;
	VK_SOCKET_INIT(that->socket, rx_fd, tx_fd);
	return vk_heap_map(&that->hd, map_addr, map_len, map_prot, map_flags, map_fd, map_offset);
}
int vk_deinit(struct that *that) {
	return vk_heap_unmap(&that->hd);
}
int vk_continue(struct that *that) {
	int rc;

	while (that->status == VK_PROC_RUN) {
		rc = vk_heap_enter(&that->hd);
		if (rc == -1) {
			return -1;
		}

		that->func(that);

		rc = vk_heap_exit(&that->hd);
		if (rc == -1) {
			return -1;
		}
	}
	return 0;
}

int vk_run(struct that *that) {
	that->status = VK_PROC_RUN;
	return vk_continue(that);
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

