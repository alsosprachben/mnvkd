#ifndef VK_STACK_H
#define VK_STACK_H

#include <errno.h>
#include <stdio.h>

struct that;

struct vk_stack {
	void *buf;
	size_t len;
	void *start;
	void *stop;
	void *cursor;
};

void vk_stack_init(struct vk_stack *stack, void *buf, size_t len);
int vk_stack_begin(struct vk_stack *stack, struct that *that);
int vk_stack_alloc(struct vk_stack *stack, struct that *that, void **p_ptr, size_t nmemb, size_t count);
int vk_stack_end();

#define vk_calloc(var, nmemb, count) vk_stack_alloc(that->stack, that, (void **) &(var), (nmemb), (count))

#endif
