#include "vk_stack.h"
#include "vk_debug.h"
#include "vk_stack_s.h"
#include "vk_wrapguard.h"

#include <errno.h>

int vk_stack_pagelen(size_t nmemb, size_t count, size_t* pagelen_ptr)
{
	int rc;
	size_t size;
	size_t len;
	size_t pagelen;

	rc = vk_safe_mult(nmemb, count, &size);
	if (rc == -1) {
		return -1;
	}

	rc = vk_safe_add(size, sizeof(size_t), &len);
	if (rc == -1) {
		return -1;
	}

	pagelen = len / vk_pagesize();

	if (pagelen * vk_pagesize() < len) {
		++pagelen;
	}

	*pagelen_ptr = pagelen;

	return 0;
}

int vk_stack_alignedlen(size_t nmemb, size_t count, size_t* alignedlen_ptr)
{
	int rc;
	size_t pagelen;

	rc = vk_safe_pagelen(nmemb, count, &pagelen);
	if (rc == -1) {
		return -1;
	}

	rc = vk_safe_mult(pagelen, vk_pagesize(), alignedlen_ptr);
	if (rc == -1) {
		return -1;
	}

	return 0;
}

void vk_stack_init(struct vk_stack* stack_ptr, void* addr, size_t len)
{
	stack_ptr->addr_start = addr;
	stack_ptr->addr_stop = addr + len;
	stack_ptr->addr_cursor = addr;
}

void* vk_stack_push(struct vk_stack* stack_ptr, size_t nmemb, size_t count)
{
	int rc;
	size_t len;
	void* addr;

	vk_kdbgf("vk_stack_push(%zu, %zu)\n", nmemb, count);
	rc = vk_safe_alignedlen(nmemb, count, &len);
	if (rc == -1) {
		return NULL;
	}

	if ((char*)stack_ptr->addr_cursor + len <= (char*)stack_ptr->addr_stop) {
		addr = stack_ptr->addr_cursor;

		/* push cursor and link */
		stack_ptr->addr_cursor = (char*)stack_ptr->addr_cursor + len;
		((size_t*)stack_ptr->addr_cursor)[-1] = len;

		vk_kdbgf("stack use = %zu/%zu\n",
			 (size_t)((char*)stack_ptr->addr_cursor - (char*)stack_ptr->addr_start),
			 (size_t)((char*)stack_ptr->addr_stop - (char*)stack_ptr->addr_start));
		return addr;
	}

	vk_kdbgf("vk_stack_push() needs page count of %zu\n",
		 ((size_t)((char*)stack_ptr->addr_cursor + len - (char*)stack_ptr->addr_start)) / 4096);
	errno = ENOMEM;
	return NULL;
}

int vk_stack_pop(struct vk_stack* stack_ptr)
{
	size_t len;
	size_t* len_ptr;

	if (stack_ptr->addr_cursor <= stack_ptr->addr_start) {
		errno = ERANGE;
		return -1;
	}

	len_ptr = (size_t*)stack_ptr->addr_cursor - 1;
	if (len_ptr < (size_t*)stack_ptr->addr_start) {
		errno = ENOENT;
		return -1;
	} else if (len_ptr >= (size_t*)stack_ptr->addr_stop) {
		errno = EINVAL;
		return -1;
	}

	/* pop cursor from link */
	len = ((size_t*)stack_ptr->addr_cursor)[-1];
	stack_ptr->addr_cursor = (char*)stack_ptr->addr_cursor - len;

	return 0;
}

void* vk_stack_get_start(struct vk_stack* stack_ptr) { return stack_ptr->addr_start; }

void* vk_stack_get_cursor(struct vk_stack* stack_ptr) { return stack_ptr->addr_cursor; }

void* vk_stack_get_stop(struct vk_stack* stack_ptr) { return stack_ptr->addr_stop; }

size_t vk_stack_get_free(struct vk_stack* stack_ptr) { return stack_ptr->addr_stop - stack_ptr->addr_cursor; }

size_t vk_stack_get_length(struct vk_stack* stack_ptr) { return stack_ptr->addr_stop - stack_ptr->addr_start; }

int vk_stack_push_stack(struct vk_stack* stack_ptr, struct vk_stack* new_stack_ptr, size_t nmemb, size_t count)
{
	void* addr;

	addr = vk_stack_push(stack_ptr, nmemb, count);
	if (addr == NULL) {
		return -1;
	}

	vk_stack_init(stack_ptr, new_stack_ptr, nmemb * count);

	return 0;
}

int vk_stack_push_stack_pages(struct vk_stack* stack_ptr, struct vk_stack* new_stack_ptr, size_t page_count)
{
	return vk_stack_push_stack(stack_ptr, new_stack_ptr, page_count, vk_pagesize());
}
