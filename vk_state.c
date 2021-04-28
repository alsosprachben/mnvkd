#include "vk_state.h"

#include "vk_heap.h"

int vk_init(struct that *that, void (*func)(struct that *that), char *file, size_t line, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset) {
	that->func = func;
	that->file = file;
	that->line = line;
	that->counter = -1;
	that->status = 0;
	that->error = 0;
	that->msg = 0;
	VK_SOCKET_INIT(that->socket);
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

#ifdef TEST_STATE

#include "vk_vectoring.h"

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

	/* vk_procdump(that, "in1"); */

	rc = snprintf(self->s1, 256, "test 1\n");
	if (rc == -1) {
		vk_error();
	}

	vk_msg((intptr_t) self->s1);

	/* vk_procdump(that, "in2"); */

	rc = snprintf(self->s2, 256, "test 2\n");
	if (rc == -1) {
		vk_error();
	}
	vk_msg((intptr_t) self->s2);

	for (self->i = 0; self->i < 1000000; self->i++) {
		vk_calloc(self->other, 5);

		rc = snprintf(self->other[3].s3, 256, "test 3: %zu\n", self->i);
		if (rc == -1) {
			vk_error();
		}

        vk_send(self->other[3].s3, strlen(self->other[3].s3));

        if (self->i % 1000 == 999) {
            vk_flush();
        }
        /*
		self->other[3].s3[0] = '\0';
		vk_msg((intptr_t) self->other[3].s3);
        */

		vk_free();
	}

	vk_end();
}

int main() {
	int rc;
	ssize_t sent;
	ssize_t received;
	struct that that;

	rc = VK_INIT_PRIVATE(&that, proc_a, 4096 * 2);
	if (rc == -1) {
		return 1;
	}

	do {
		/*vk_procdump(&that, "out"); */
		that.status = VK_PROC_RUN;
		vk_continue(&that);
        if (that.status == VK_PROC_WAIT) {
            switch (that.socket.block.op) {
                case VK_OP_WRITE:
                    sent = vk_vectoring_write(&that.socket.tx.ring, 1);
                    if (sent == -1) {
                        return 2;
                    }
                    break;
                case VK_OP_READ:
                    received = vk_vectoring_read(&that.socket.rx.ring, 0);
                    if (sent == -1) {
                        return 3;
                    }
            }
        }
	} while (that.status != VK_PROC_END);

	return 0;
}

#endif