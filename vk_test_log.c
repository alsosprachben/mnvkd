#define DEBUG 1

#include "vk_main_local.h"

void logging(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_logf("LOG test %d\n", 1);
	vk_log("LOG test");
	errno = EINVAL;
	vk_perror("LOG test");

	vk_dbgf("LOG debug %d\n", 1);
	vk_dbg("LOG debug");
	errno = EINVAL;
	vk_dbg_perror("LOG debug");

	vk_end();
}

int main() {
	return vk_main_local_init(logging, NULL, 0, 34);
}