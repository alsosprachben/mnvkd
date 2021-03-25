#ifndef VK_SPACE_H
#define VK_SPACE_H

#include <string.h>

#include "vk_state.h"
#include "vk_stack.h"

struct vk_space_1k {
	struct that that;
	struct vk_stack stack;
	char buf[1024];
};

struct vk_space_4k {
	struct that that;
	struct vk_stack stack;
	char buf[4096];
};

struct vk_space_32k {
	struct that that;
	struct vk_stack stack;
	char buf[32768];
};

#define VK_SPACE_INIT(space_ptr, vk_func) {            \
	memset((space_ptr), 0, sizeof *(space_ptr));   \
	vk_stack_init(                                 \
		      &(space_ptr)->stack,             \
		      &(space_ptr)->stack.buf,         \
		sizeof (space_ptr)->buf                \
	);                                             \
	vk_init(&(space_ptr)->that, (vk_func));        \
	(space_ptr)->that.stack = &(space_ptr)->stack; \
}

#endif
