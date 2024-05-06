#include <stdio.h>

#include "vk_main_local.h"

void example2(struct vk_thread *that);

void example1(struct vk_thread *that)
{
	struct {
		struct vk_thread* example_ptr; /* main coroutine passed as an argument */
		struct vk_thread* example2_ptr;
		int i;
	}* self;
	vk_begin();

	vk_calloc_size(self->example2_ptr, 1, vk_alloc_size());
	vk_child(self->example2_ptr, example2);
	vk_copy_arg(self->example2_ptr, &that, sizeof (that));

	for (self->i = 0; self->i < 10; self->i++) {
		dprintf(1, "example1: %i\n", self->i);
		vk_call(self->example2_ptr);
	}

	vk_free();

	vk_call(self->example_ptr); /* back to main */

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
		dprintf(1, "example2: %i\n", self->i);
		vk_call(self->example1_vk);
	}

	vk_end();
}

void example(struct vk_thread *that)
{
	struct {
		int i;
		struct vk_thread *example1_ptr;
	} *self;
	vk_begin();

	vk_calloc_size(self->example1_ptr, 1, vk_alloc_size());
	vk_child(self->example1_ptr, example1);
	vk_copy_arg(self->example1_ptr, &that, sizeof (that));
	vk_call(self->example1_ptr);

	vk_free();

	vk_end();
}

int main() {
	return vk_local_main_init(example, NULL, 0, 34);
}

