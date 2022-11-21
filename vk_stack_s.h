#ifndef VK_STACK_S_H
#define VK_STACK_S_H

struct vk_stack {
	/*** ALLOCATIONS ***/
	/* CONSTRAINT: page_start <= page_cursor <= page_stop */

	/* pointer to the start of the heap */
	void  *addr_start;

	/* pointer to the start of the next available allocation */
	void  *addr_cursor;

	/* pointer to the first address after the heap */
	void  *addr_stop;
};

#endif