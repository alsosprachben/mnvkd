#include "vk_main.h"
#include "vk_thread.h"

void pollreading(struct vk_thread* that) {
	ssize_t rc = 0;
	struct {
		char line[1024];
		ssize_t size;
	} *self;

	vk_begin();

	vk_pollread(self->size);
	if (self->size > sizeof (self->line) - 1) {
		vk_raise(ERANGE);
	}
	vk_logf("LOG size of 1st chunk: %zu\n", self->size);

	vk_read(rc, self->line, self->size);
	if (rc != self->size) {
		vk_raise(EPIPE);
	}

	vk_pollread(self->size);
	if (self->size > sizeof (self->line) - 1) {
		vk_raise(ERANGE);
	}
	vk_logf("LOG size of 2nd chunk: %zu\n", self->size);

	vk_read(rc, self->line, self->size);
	if (rc != self->size) {
		vk_raise(EPIPE);
	}

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG pollreading");
	}

	vk_end();
}

int main(int argc, char* argv[])
{
	return vk_main_init(pollreading, NULL, 0, 26, 0, 1);
}