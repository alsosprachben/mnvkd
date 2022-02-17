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

		/* fprintf(stderr, "that->msg = %s\n", (char *) that->msg); */

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
	return that->status != VK_PROC_END;
}

int vk_sync_unblock(struct that *that) {
	ssize_t sent;
	ssize_t received;

	switch (that->status) {
		case VK_PROC_WAIT:
			if (that->waiting_socket_ptr != NULL) {
				switch (that->waiting_socket_ptr->block.op) {
					case VK_OP_WRITE:
						sent = vk_vectoring_write(&that->waiting_socket_ptr->tx.ring, that->waiting_socket_ptr->rx_fd);
						if (sent == -1) {
							return -1;
						}
						break;
					case VK_OP_READ:
						received = vk_vectoring_read(&that->waiting_socket_ptr->rx.ring, that->waiting_socket_ptr->tx_fd);
						if (received == -1) {
							return -1;
						}
						break;
				}
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

#ifdef TEST_STATE

void proc_a(struct that *that) {
	int rc;

	struct {
		int rc;
		size_t i;
		char s1[256];
		char s2[256];
		struct {
			char s3[256];
		} *other;
	} *self;

	vk_begin();

	for (self->i = 0; self->i < 1000000; self->i++) {
		vk_calloc(self->other, 5);

		vk_readline(rc, self->other[2].s3, sizeof (self->other[2].s3) - 1);
		if (rc == 0 || vk_eof()) {
			vk_free();
			break;
		}

		self->other[2].s3[rc] = '\0';

		rc = snprintf(self->other[3].s3, sizeof (self->other[3].s3) - 1, "Line %zu: %s", self->i, self->other[2].s3);
		if (rc == -1) {
			vk_error();
		}

		vk_write(self->other[3].s3, strlen(self->other[3].s3));
		vk_flush();

		vk_free();
	}

	vk_end();
}

int main() {
	int rc;
	struct that that;

	rc = VK_INIT_PRIVATE(&that, proc_a, 0, 1, 4096 * 2);
	if (rc == -1) {
		return 1;
	}

	do {
		vk_run(&that);
		rc = vk_sync_unblock(&that);
		if (rc == -1) {
			return -1;
		}
	} while (vk_runnable(&that));;

	rc = vk_deinit(&that);
	if (rc == -1) {
		return 2;
	}

	return 0;
}

#endif
