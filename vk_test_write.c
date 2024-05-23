#include <stdio.h>

#include "vk_main_local.h"

void writing(struct vk_thread *that)
{
	int rc = 0;
	struct {
		char line[1024];
		char buf[1024];
		size_t size;
	}* self;
	vk_begin();

	strncpy(self->buf, "1234567890", sizeof (self->buf) - 1);
	self->size = strlen(self->buf);

	vk_write_literal("MyProto 1.0\n");
	vk_writef(rc, self->line, sizeof (self->line) - 1, "%zu\n", self->size);
	vk_write(self->buf, self->size);
	vk_flush();

	vk_finally();
	if (errno != 0) {
		vk_perror("writing");
	}

	vk_end();
}

int main() {
	return vk_main_local_init(writing, NULL, 0, 34);
}