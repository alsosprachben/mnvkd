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
	if (strcmp(self->line, "MyProto 1.0\n") != 0) {
		vk_raise(EINVAL);
	}

	vk_readline(rc, self->line, sizeof (self->line) - 1);
	rc = sscanf(self->line, "%zu", &self->size);
	if (rc != 1) {
		vk_perror("sscanf");
		vk_error();
	}

	if (self->size > sizeof (self->buf) - 1) {
		vk_raise(ERANGE);
	}

	vk_logf("LOG header size: %zu\n", self->size);

	vk_read(rc, self->buf, self->size);
	if (rc != self->size) {
		vk_raise(EPIPE);
	}

	vk_logf("LOG body: %s\n", self->buf);

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG reading");
	}

	vk_end();
}

int main() {
	return vk_main_local_init(reading, NULL, 0, 34);
}