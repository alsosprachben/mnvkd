#include "vk_main_local.h"

void forwarding(struct vk_thread *that)
{
	int rc = 0;
	struct {
		char line[1024];
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

	vk_logf("LOG header size: %zu\n", self->size);

	vk_write_literal("MyProto 1.0\n");
	vk_writef(rc, self->line, sizeof (self->line) - 1, "%zu\n", self->size);

	vk_forward(rc, self->size);
	vk_flush();

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG forwarding");
	}

	vk_end();
}

int main() {
	return vk_main_local_init(forwarding, NULL, 0, 34);
}