#ifndef VK_FUTURE_S_H
#define VK_FUTURE_S_H

#include <stdint.h>
#include <sys/queue.h>
struct vk_future;
struct vk_thread;

/* Inter-coroutine message passing, between the coroutines within a single heap. */
struct vk_future {
	TAILQ_ENTRY(vk_future) ft_list_elem;
	struct vk_thread *vk;
	void *msg;
	int error; /* errno */
};


#endif