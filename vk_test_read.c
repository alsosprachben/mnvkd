#include <stdio.h>

#include "vk_main_local.h"

void reading(struct vk_thread *that)
{
	int rc = 0;

	struct {
		char line[1024];
		char buf[1024];
		size_t size;
	}* self;
	vk_begin();

	vk_readline(rc, self->line, sizeof (self->line) - 1);
	rc = sscanf(self->line, "%zu", &self->size);
	if (rc != 1) {
		vk_perror("sscanf");
		vk_error();
	}

	if (self->size > sizeof (self->buf) - 1) {
		vk_raise(ERANGE);
	}

	dprintf(1, "header size: %zu\n", self->size);

	vk_read(rc, self->buf, self->size);
	if (rc != self->size) {
		vk_raise(EPIPE);
	}

	dprintf(1, "body: %s\n", self->buf);

	vk_finally();
	if (errno != 0) {
		vk_perror("reading");
	}

	vk_end();
}

int main() {
	return vk_local_main_init(reading, NULL, 0, 34);
}