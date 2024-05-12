#define DEBUG 1

#include "vk_main_local.h"

void logging(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_logf("test %d\n", 1);
	vk_log("test");
	errno = EINVAL;
	vk_perror("test");

	vk_dbgf("debug %d\n", 1);
	vk_dbg("debug");
	errno = EINVAL;
	vk_dbg_perror("debug");

	vk_end();
}

int main() {
	return vk_local_main_init(logging, NULL, 0, 34);
}