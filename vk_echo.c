#include "vk_thread.h"
#include "vk_service_s.h"
#include "vk_debug.h"

void vk_echo(struct vk_thread* that)
{
	int rc = 0;

	struct {
		struct vk_service service; /* via vk_copy_arg() */
		size_t i;
		struct {
			char in[8192];
			char out[8192];
		}* buf; /* pointer to demo dynamic allocation */
	}* self;

	vk_begin();

	for (self->i = 0;; self->i++) {
		vk_calloc(self->buf, 1); /* demo dynamic allocation in a loop */

		vk_readline(rc, self->buf->in, sizeof(self->buf->in) - 1);
		if (rc == 0 || vk_eof() || rc > sizeof(self->buf->in) - 1) {
			vk_dbgf("rc=%d eof=%d", rc, vk_eof());
			vk_free();
			break;
		}

		self->buf->in[rc] = '\0';

		rc = snprintf(self->buf->out, sizeof(self->buf->out) - 1, "Line %zu: %s", self->i, self->buf->in);
		if (rc == -1) {
			vk_error();
		}

		vk_write(self->buf->out, rc);
		vk_flush();

		vk_free(); /* demo deallocating each loop */
	}

	vk_end();
}
