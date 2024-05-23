#include <stdio.h>

#include "vk_main_local.h"

void example(struct vk_thread *that) {
	struct {
		struct blah {
			/* members dynamically allocated */
			int i;
			char *str; /* allocated inside the loop */
		} *blah_ptr;
		int j;
	} *self;
	vk_begin();

	vk_calloc(self->blah_ptr, 2);

	self->j = 0;
	for (self->j = 0; self->j < 2; self->j++) {

		/* use blah_ptr[self->j] */

		vk_calloc(self->blah_ptr[self->j].str, 50);
		snprintf(self->blah_ptr[self->j].str, 50 - 1, "Dynamically allocated for %i",
		         self->j);

		for (self->blah_ptr[self->j].i = 0;
		     self->blah_ptr[self->j].i < 10; self->blah_ptr[self->j].i++) {
			vk_logf("LOG counters: (%i, %i, %s)\n", self->j, self->blah_ptr[self->j].i,
			        self->blah_ptr[self->j].str);
		}

		vk_free(); /* blah_ptr[self->j].str was at the top of the stack */

	}

	vk_free(); /* self->blah_ptr was on the top of the stack */

	vk_end();
}

int main() {
	return vk_main_local_init(example, NULL, 0, 34);
}