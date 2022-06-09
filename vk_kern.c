#include <string.h>

#include "vk_kern.h"
#include "vk_kern_s.h"

void vk_kern_clear(struct vk_kern *kern_ptr) {
    memset(kern_ptr, 0, sizeof (*kern_ptr));
}

struct vk_kern *vk_kern_alloc(struct vk_heap_descriptor *hd_ptr) {
    struct vk_kern *kern_ptr;
    int rc;
    int i;

	rc = vk_heap_map(hd_ptr, NULL, 4096 * 223, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if (rc == -1) {
		return NULL;
	}

	kern_ptr = vk_heap_push(hd_ptr, vk_kern_alloc_size(), 1);
	if (kern_ptr == NULL) {
		return NULL;
	}

    kern_ptr->hd_ptr = hd_ptr;

    SLIST_INIT(&kern_ptr->free_procs);

    // build free-list from top-down
    for (i = VK_KERN_PROC_MAX - 1; i >= 0; i--) {
        SLIST_INSERT_HEAD(&kern_ptr->free_procs, &kern_ptr->proc_table[i], free_list_elem);
    }

    kern_ptr->proc_count = 0;
    kern_ptr->nfds = 0;

    return kern_ptr;
}

size_t vk_kern_alloc_size() {
    return sizeof (struct vk_kern);
}

struct vk_proc *vk_kern_alloc_proc(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    if (SLIST_EMPTY(&kern_ptr->free_procs)) {
        errno = ENOMEM;
        return NULL;
    }

    proc_ptr = SLIST_FIRST(&kern_ptr->free_procs);
    SLIST_REMOVE_HEAD(&kern_ptr->free_procs, free_list_elem);

    return proc_ptr;
}

void vk_kern_free_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    SLIST_INSERT_HEAD(&kern_ptr->free_procs, proc_ptr, free_list_elem);
}