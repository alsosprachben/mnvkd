#include "vk_stack.h"
#include "vk_stack_s.h"
#include "debug.h"

#include "errno.h"

#define BLAH_VK_PAGESIZE 4096
size_t _vk_pagesize() {
	static long page_size;
	static int page_size_set = 0;
	if (! page_size_set) {
		page_size = sysconf(_SC_PAGE_SIZE);
		page_size_set = 1;
	}

	return (size_t) page_size;
}
size_t vk_pagesize() {
#ifdef VK_PAGESIZE
	return VK_PAGESIZE;
#else
	return _vk_pagesize();
#endif 
}

size_t vk_blocklen(size_t nmemb, size_t count) {
	size_t len;
	size_t blocklen;

	/* array of count, plus enough space for the linking len to pop to the start */
	len = nmemb * count + sizeof (size_t);
	blocklen = len / vk_pagesize();
	blocklen *= vk_pagesize();
	if (blocklen < len) {
		blocklen += vk_pagesize();
	}

	return blocklen;
}

void vk_stack_init(struct vk_stack *stack_ptr, void *addr, size_t len) {
    stack_ptr->addr_start = addr;
    stack_ptr->addr_stop = addr + len;
    stack_ptr->addr_cursor = addr;
}

void *vk_stack_push(struct vk_stack *stack_ptr, size_t nmemb, size_t count) {
	size_t len;
	void *addr;

	DBG("vk_heap_push(%zu, %zu)\n", nmemb, count);
	len = vk_blocklen(nmemb, count);

	if ((char *) stack_ptr->addr_cursor + len <= (char *) stack_ptr->addr_stop) {
		addr = stack_ptr->addr_cursor;

		/* push cursor and link */
		stack_ptr->addr_cursor = (char *) stack_ptr->addr_cursor + len;
		((size_t *) stack_ptr->addr_cursor)[-1] = len;

		DBG("heap use = %zu/%zu\n", (size_t) ((char *) stack_ptr->addr_cursor - (char *) stack_ptr->addr_start), (size_t) ((char *) stack_ptr->addr_stop - (char *) stack_ptr->addr_start));
		return addr;
	}
	
	DBG("vk_stack_push() needs page count of %zu\n", ((size_t) ((char *) stack_ptr->addr_cursor + len - (char *) stack_ptr->addr_start)) / 4096);
	errno = ENOMEM;
	return NULL;
}

int vk_stack_pop(struct vk_stack *stack_ptr) {
	size_t len;
	size_t *len_ptr;

	len_ptr = (size_t *) stack_ptr->addr_cursor - 1;
	if (len_ptr < (size_t *) stack_ptr->addr_start) {
		errno = ENOENT;
		return -1;
	} else if (len_ptr >= (size_t *) stack_ptr->addr_stop) {
		errno = EINVAL;
		return -1;
	}

	/* pop cursor from link */
	len = ((size_t *) stack_ptr->addr_cursor)[-1];
	stack_ptr->addr_cursor = (char *) stack_ptr->addr_cursor - len;

	return 0;
}

void *vk_stack_get_start(struct vk_stack *stack_ptr) {
	return stack_ptr->addr_start;
}

void *vk_stack_get_cursor(struct vk_stack *stack_ptr) {
	return stack_ptr->addr_cursor;
}

void *vk_stack_get_stop(struct vk_stack *stack_ptr) {
	return stack_ptr->addr_stop;
}

size_t vk_stack_get_free(struct vk_stack *stack_ptr) {
	return stack_ptr->addr_stop - stack_ptr->addr_cursor;
}
