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
#include "vk_heap.h"
#include "vk_kern.h"
#include "vk_proc.h"

int main2(int argc, char *argv[]) {
	int rc;
	struct vk_heap_descriptor *kern_heap_ptr;
	struct vk_kern *kern_ptr;
	struct vk_proc *proc_ptr;
	struct that *vk_ptr;

	kern_heap_ptr = calloc(1, vk_heap_alloc_size());
	kern_ptr = vk_kern_alloc(kern_heap_ptr);
	if (kern_ptr == NULL) {
		return 1;
	}

	proc_ptr = vk_kern_alloc_proc(kern_ptr);
	if (proc_ptr == NULL) {
		return 1;
	}
	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 24);
	if (rc == -1) {
		return 1;
	}

	vk_ptr = vk_proc_alloc_that(proc_ptr);
	if (vk_ptr == NULL) {
		return 1;
	}

	fcntl(0, F_SETFL, O_NONBLOCK);
	fcntl(1, F_SETFL, O_NONBLOCK);

	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 15);
	if (rc == -1) {
		return 1;
	}

	VK_INIT(vk_ptr, proc_ptr, echo, 0, 1);

	vk_proc_enqueue_run(proc_ptr, vk_ptr);
	vk_kern_flush_proc_queues(kern_ptr, proc_ptr);

	rc = vk_kern_loop(kern_ptr);
	if (rc == -1) {
		return -1;
	}

	rc = vk_deinit(vk_ptr);
	if (rc == -1) {
		return 4;
	}

	rc = vk_proc_deinit(proc_ptr);
	if (rc == -1) {
		return 5;
	}

	vk_kern_free_proc(kern_ptr, proc_ptr);

	return 0;
}