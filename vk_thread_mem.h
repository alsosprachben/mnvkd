#ifndef VK_THREAD_MEM_H
#define VK_THREAD_MEM_H

/* allocation via the heap */

/* allocate an array of the pointer from the micro-heap as a stack */
#define vk_calloc(val_ptr, nmemb) \
	(val_ptr) = vk_heap_push(vk_proc_get_heap(vk_get_proc(that)), (nmemb), sizeof (*(val_ptr))); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

#define vk_calloc_size(val_ptr, size, nmemb) \
	(val_ptr) = vk_heap_push(vk_proc_get_heap(vk_get_proc(that)), (nmemb), size); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

/* de-allocate the last array from the micro-heap stack */
#define vk_free()                                          \
	rc = vk_heap_pop(vk_proc_get_heap(vk_get_proc(that))); \
	if (rc == -1) {                                        \
		if (vk_get_counter(that) == -2) {                         \
			vk_perror("vk_free");                          \
		} else {                                           \
			vk_error();                                    \
		}                                                  \
	}

#endif