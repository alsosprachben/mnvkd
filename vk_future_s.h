#ifndef VK_FUTURE_S_H
#define VK_FUTURE_S_H

#include <stdint.h>
struct vk_future;
struct that;

/* Inter-coroutine message passing, between the coroutines within a single heap. */
struct vk_future {
	struct vk_future *next; /* for reentrance */
	struct that *vk;
	void *msg;
	int error; /* errno */
};


#endif