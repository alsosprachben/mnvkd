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

	rc = vk_heap_map(hd_ptr, NULL, 4096 * 235, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
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
        kern_ptr->proc_table[i].proc_id = i;
        kern_ptr->proc_table[i].kern_ptr = kern_ptr;
    }

    kern_ptr->proc_count = 0;
    kern_ptr->nfds = 0;

    kern_ptr->event_proc_next_pos = 0;

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

void vk_kern_enqueue_run(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    SLIST_INSERT_HEAD(&kern_ptr->run_procs, proc_ptr, run_list_elem);
}

void vk_kern_enqueue_blocked(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    SLIST_INSERT_HEAD(&kern_ptr->blocked_procs, proc_ptr, blocked_list_elem);
}

void vk_kern_drop_run(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    SLIST_REMOVE(&kern_ptr->run_procs, proc_ptr, vk_proc, run_list_elem);
}

void vk_kern_drop_blocked(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    SLIST_REMOVE(&kern_ptr->blocked_procs, proc_ptr, vk_proc, blocked_list_elem);
}

struct vk_proc *vk_kern_dequeue_run(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    if (SLIST_EMPTY(&kern_ptr->run_procs)) {
        return NULL;
    }

    proc_ptr = SLIST_FIRST(&kern_ptr->run_procs);
    SLIST_REMOVE_HEAD(&kern_ptr->run_procs, run_list_elem);

    return proc_ptr;
}

struct vk_proc *vk_kern_dequeue_blocked(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    if (SLIST_EMPTY(&kern_ptr->blocked_procs)) {
        return NULL;
    }

    proc_ptr = SLIST_FIRST(&kern_ptr->blocked_procs);
    SLIST_REMOVE_HEAD(&kern_ptr->blocked_procs, blocked_list_elem);

    return proc_ptr;
}

void vk_kern_prepoll_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    struct vk_kern_event_index *event_index_ptr;

    if (proc_ptr->nfds == 0) {
        /* Skip adding poll events if none needed. */
        return;
    }

    event_index_ptr = &kern_ptr->event_index[proc_ptr->proc_id];
    event_index_ptr->proc_id = proc_ptr->proc_id;
    event_index_ptr->event_start_pos = kern_ptr->event_proc_next_pos;
    event_index_ptr->nfds = proc_ptr->nfds;
    memcpy(&kern_ptr->events[event_index_ptr->event_start_pos], proc_ptr->fds, sizeof (struct pollfd) * event_index_ptr->nfds);
    kern_ptr->event_proc_next_pos += event_index_ptr->nfds;
}

void vk_kern_postpoll_proc_exec(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {

}

void vk_kern_prepoll(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    while ((proc_ptr = vk_kern_dequeue_blocked(kern_ptr))) {
        vk_kern_prepoll_proc(kern_ptr, proc_ptr);
    }
}

void vk_kern_postpoll(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    /* traverse event index, then dispatch */


    while ((proc_ptr = vk_kern_dequeue_run(kern_ptr))) {
        vk_kern_postpoll_proc_exec(kern_ptr, proc_ptr);
    }
}