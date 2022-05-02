#ifndef VK_HEAP_S_H
#define VK_HEAP_S_H

#include <unistd.h>

struct mmapping {
	/* To Represent a Physical Memory Mapping via its current mmap() arguments */

	/* mmap() return value */
	void *retval;

	void *addr;
	size_t len;
	int prot;
	int flags;
	int fd;
	off_t offset;
};

struct vk_heap_descriptor {
	/*** MAPPING ***/

	/* Represents the current physical memory mapping state via mmap() and mprotect() */
	struct mmapping mapping;

	/* Memory mapping protection flags from the point of view of inside the heap: 
	 *  - This is what in-heap code needs to execute.
	 *  - This is what protection to set before continuing code inside the heap.
	 */
	int prot_inside;

	/* Memory mapping protection flags from the point of view of outside the heap:
	 *  - This is what off-heap code needs to be visible to execute.
	 *  - This is what protection to set after code inside the heap yields.
	 */
	int prot_outside;

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
