#ifndef VK_POOL_S_H
#define VK_POOL_S_H

#include "vk_heap_s.h"
#include "vk_pool.h"
#include "vk_queue.h"

struct vk_pool_entry {
	size_t entry_id;
	SLIST_ENTRY(vk_pool_entry) free_list_elem;
	struct vk_heap heap;
};

struct vk_pool {
	struct vk_heap pool_heap;				   /* for the array of struct vk_pool_entry */
	SLIST_HEAD(free_entries_head, vk_pool_entry) free_entries; /* head of the freelist */
	size_t object_size;					   /* each entry of memory is this size */
	size_t object_count;					   /* number of memory entries */
	void* buf;				  /* optional buffer, otherwise allocated from vk_heap */
	size_t buf_len;				  /* buffer size, must be determined by vk_pool_needed_buffer_size() */
	vk_pool_entry_init_func object_init_func; /* when  */
	void* object_init_func_udata;		  /*  */
	vk_pool_entry_init_func object_free_func; /* when  */
	void* object_free_func_udata;		  /*  */
	vk_pool_entry_init_func object_deinit_func; /* when  */
	void* object_deinit_func_udata;		    /*  */
};

#endif
