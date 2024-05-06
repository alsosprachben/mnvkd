#include <stdio.h>

#include "vk_main_local.h"

void example(struct vk_thread *that)
{
	struct {
		int i;
	} *self;
	vk_begin();
	for (self->i = 0; self->i < 10; self->i++) {
		dprintf(1, "iteration %i\n", self->i);
		vk_yield(VK_PROC_YIELD); /* low-level of vk_pause() */
	}
	vk_end();
}

int main() {
	return vk_local_main_init(example, NULL, 0, 26);
}
