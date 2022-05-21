#include "vk_state.h"
#include "debug.h"

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

#include "vk_proc.h"
#include <fcntl.h>
#include <stdlib.h>
#include "vk_proc.h"

int main2(int argc, char *argv[]) {
	int rc;
	int rx_fd;
	struct vk_proc *proc_ptr;
	struct that *vk_ptr;

	proc_ptr = calloc(1, vk_proc_alloc_size());
	vk_ptr = calloc(1, vk_alloc_size());

	if (argc >= 2) {
		rc = open(argv[1], O_RDONLY);
	} else {
		rc = open("http_request_pipeline.txt", O_RDONLY);
	}
	rx_fd = rc;
	fcntl(rx_fd, F_SETFL, O_NONBLOCK);
	fcntl(0,  F_SETFL, O_NONBLOCK);

	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 23);
	if (rc == -1) {
		return 1;
	}

	VK_INIT(vk_ptr, proc_ptr, proc_a, rx_fd, 1);

	vk_proc_enqueue_run(proc_ptr, vk_ptr);
	do {
		rc = vk_proc_execute(proc_ptr);
		if (rc == -1) {
			return 2;
		}
		rc = vk_proc_poll(proc_ptr);
		if (rc == -1) {
			return 3;
		}
	} while (vk_proc_pending(proc_ptr));

	rc = vk_deinit(vk_ptr);
	if (rc == -1) {
		return 4;
	}

	rc = vk_proc_deinit(proc_ptr);
	if (rc == -1) {
		return 5;
	}

	return 0;
}


