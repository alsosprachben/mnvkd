#include "vk_state.h"

void echo(struct that *that) {
	int rc;

	struct {
		size_t i;
		struct {
			char in[8192];
			char out[8192];
		} *buf;
	} *self;

	vk_begin();

	for (self->i = 0; ; self->i++) {
		vk_calloc(self->buf, 1);

		vk_readline(rc, self->buf->in, sizeof (self->buf->in) - 1);
		if (rc == 0 || vk_eof()) {
			vk_free();
			break;
		}

		self->buf->in[rc] = '\0';

		rc = snprintf(self->buf->out, sizeof (self->buf->out) - 1, "Line %zu: %s", self->i, self->buf->in);
		if (rc == -1) {
			vk_error();
		}

		vk_write(self->buf->out, strlen(self->buf->out));
		vk_flush();

		vk_free();
	}

	vk_end();
}

#include <fcntl.h>
#include <stdlib.h>
#include "vk_proc.h"

int main2(int argc, char *argv[]) {
	int rc;
	struct vk_proc *proc_ptr;
	struct that *vk_ptr;

	proc_ptr = calloc(1, vk_proc_alloc_size());
	vk_ptr = calloc(1, vk_alloc_size());

	fcntl(0, F_SETFL, O_NONBLOCK);
	fcntl(1, F_SETFL, O_NONBLOCK);

	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 15);
	if (rc == -1) {
		return 1;
	}

	VK_INIT(vk_ptr, proc_ptr, echo, 0, 1);

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