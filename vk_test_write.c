#include <stdio.h>

#include "vk_main_local.h"

void writing(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_write_literal("10\n");
	vk_write_literal("1234567890");
	vk_flush();

	vk_end();
}

int main() {
	return vk_local_main_init(writing, NULL, 0, 34);
}