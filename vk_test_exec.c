#include "vk_main_local.h"

void example2(struct vk_thread *that);

void example1(struct vk_thread *that)
{
	struct {
		struct vk_thread* example2_ptr;
		int i;
	}* self;
	vk_begin();

	vk_calloc_size(self->example2_ptr, 1, vk_alloc_size());
	vk_child(self->example2_ptr, example2);
	vk_copy_arg(self->example2_ptr, &that, sizeof (that));

	for (self->i = 0; self->i < 10; self->i++) {
		vk_logf("LOG example1: %i\n", self->i);
		vk_call(self->example2_ptr);
	}

	vk_free(); /* free self->example2_ptr */

	vk_end();
}

void example2(struct vk_thread *that)
{
	struct {
		struct vk_thread* example1_vk; /* other coroutine passed as argument */
		int i;
	}* self;
	vk_begin();

	for (self->i = 0; self->i < 10; self->i++) {
		vk_logf("LOG example2: %i\n", self->i);
		vk_call(self->example1_vk);
	}

	vk_end();
}

int main() {
	return vk_main_local_init(example1, NULL, 0, 44);
}