#ifndef VK_THREAD_MEM_H
#define VK_THREAD_MEM_H

/* allocation via the heap */

/* allocate an array of the pointer from the micro-heap as a stack */
#define vk_calloc(val_ptr, nmemb) \
	(val_ptr) = vk_stack_push(vk_proc_local_get_stack(vk_get_proc_local(that)), (nmemb), sizeof (*(val_ptr))); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

#define vk_calloc_size(val_ptr, nmemb, size) \
	(val_ptr) = vk_stack_push(vk_proc_local_get_stack(vk_get_proc_local(that)), (nmemb), (size)); \
	if ((val_ptr) == NULL) { \
		vk_error(); \
	}

/* de-allocate the last array from the micro-heap stack */
#define vk_free()                                          \
	rc = vk_stack_pop(vk_proc_local_get_stack(vk_get_proc_local(that))); \
	if (rc == -1) {                                        \
		if (vk_get_counter(that) == -2) {                         \
			vk_perror("vk_free");                          \
		} else {                                           \
			vk_error();                                    \
		}                                                  \
	}

#endif