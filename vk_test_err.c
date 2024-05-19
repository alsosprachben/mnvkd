#include <stdio.h>

#include "vk_main_local.h"

void erring(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_log("LOG Before error is raised.");
	errno = ENOENT;
	vk_perror("LOG somefunction");
	vk_error();

	vk_log("LOG After error is raised.");

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG Caught error");
		vk_lower();
	} else {
		vk_log("LOG Error is cleaned up.");
	}

	vk_end();
}

int main() {
	return vk_local_main_init(erring, NULL, 0, 34);
}