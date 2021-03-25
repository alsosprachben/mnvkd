#include "vk_stack.h"

size_t safe_size(size_t nmemb, size_t count) {
	/* XXX: add OpenBSD integer overflow protection code */
	return nmemb * count;
}

void vk_stack_init(struct vk_stack *stack, void *buf, size_t len) {
	stack->buf = buf;
	stack->len = len;
	stack->cursor = stack->buf;
	stack->stop = stack->cursor + stack->len;
}

int vk_stack_begin(struct vk_stack *stack, struct that *that) {
	stack->cursor = stack->buf;
	stack->stop = stack->cursor + stack->len;
	return 0;
}

int vk_stack_alloc(struct vk_stack *stack, struct that *that, void **p_ptr, size_t nmemb, size_t count) {
	size_t len;

	len = safe_size(nmemb, count);

	if (stack->cursor + len < stack->stop) {
		*p_ptr = stack->cursor;
		stack->cursor += len;
	} else {
		*p_ptr = NULL;
		errno = ENOMEM;
		return -1;
	}

	return 0;
}

int vk_stack_end(struct vk_stack *stack) {
	stack->cursor = stack->buf;
	return 0;
}
