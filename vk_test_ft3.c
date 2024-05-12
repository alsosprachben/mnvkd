#include <stdio.h>

#include "vk_main_local.h"

void background(struct vk_thread *that);

void foreground(struct vk_thread *that)
{
	struct {
		struct vk_thread* background_vk_ptr;
	}* self;
	vk_begin();

	vk_calloc_size(self->background_vk_ptr, 1, vk_alloc_size());
	vk_child(self->background_vk_ptr, background);
	vk_play(self->background_vk_ptr);

	vk_log("LOG foreground");

	vk_pause();

	vk_log("LOG not reached");

	vk_free(); /* free self->background_vk_ptr */

	vk_end();
}

void background(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_log("LOG background");

	vk_end();
}

int main() {
	return vk_local_main_init(foreground, NULL, 0, 34);
}