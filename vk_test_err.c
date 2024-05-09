#include <stdio.h>

#include "vk_main_local.h"

void erring(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_log("ERR Before error is raised.");
	errno = ENOENT;
	vk_perror("ERR somefunction");
	vk_error();

	vk_log("ERR After error is raised.");

	vk_finally();
	if (errno != 0) {
		vk_perror("ERR Caught error");
		vk_lower();
	} else {
		vk_log("ERR Error is cleaned up.");
	}

	vk_end();
}

int main() {
	return vk_local_main_init(erring, NULL, 0, 34);
}