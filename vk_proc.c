#include "debug.h"

#include "vk_proc.h"
#include "vk_proc_s.h"

#include "vk_heap.h"

int vk_proc_init(struct vk_proc *proc_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset) {
    int rc;

    rc = vk_heap_map(&proc_ptr->heap, map_addr, map_len, map_prot, map_flags, map_fd, map_offset);
    if (rc == -1) {
        return -1;
    }

    SLIST_INIT(&proc_ptr->run_q);
    SLIST_INIT(&proc_ptr->blocked_q);

    return rc;
}

int vk_proc_deinit(struct vk_proc *proc_ptr) {
    int rc;

    rc = vk_heap_unmap(&proc_ptr->heap);
    if (rc == -1) {
        return -1;
    }

    SLIST_EMPTY(&proc_ptr->run_q);
    SLIST_EMPTY(&proc_ptr->blocked_q);

    return rc;
}

struct vk_heap_descriptor *vk_proc_get_heap(struct vk_proc *proc_ptr) {
    return &proc_ptr->heap;
}

void vk_proc_enqueue_run(struct vk_proc *proc_ptr, struct that *that) {
    SLIST_INSERT_HEAD(&proc_ptr->run_q, that, run_q_elem);
}

void vk_proc_enqueue_blocked(struct vk_proc *proc_ptr, struct that *that) {
    SLIST_INSERT_HEAD(&proc_ptr->blocked_q, that, blocked_q_elem);
}

struct that *vk_proc_dequeue_run(struct vk_proc *proc_ptr) {
    struct that *that;
    that = SLIST_FIRST(&proc_ptr->run_q);
    if (that != NULL) {
        SLIST_REMOVE(&proc_ptr->run_q, that, that, run_q_elem);
    }
    return that;
}

struct that *vk_proc_dequeue_blocked(struct vk_proc *proc_ptr) {
    struct that *that;
    that = SLIST_FIRST(&proc_ptr->blocked_q);
    if (that != NULL) {
        SLIST_REMOVE_HEAD(&proc_ptr->blocked_q, blocked_q_elem);
    }
    return that;
}



int vk_proc_execute(struct vk_proc *proc_ptr, struct that *that) {
	int rc;
	DBG("--vk_execute("PRIvk")\n", ARGvk(that));

	rc = vk_heap_enter(vk_proc_get_heap(proc_ptr));
	if (rc == -1) {
		return -1;
	}
	while (vk_is_ready(that)) {
		do {
			do {
				DBG("  EXEC@"PRIvk"\n", ARGvk(that));
				that->func(that);
				DBG("  STOP@"PRIvk"\n", ARGvk(that));
				rc = vk_unblock(that);
				if (rc == -1) {
					return -1;
				}
			} while (vk_is_ready(that));

			if (vk_is_yielding(that)) {
				/* 
				 * Yielded coroutines are already added to the end of the run queue,
				 * but are left in yield state to break out of the preceeding loop,
				 * and need to be set back to run state once past the preceeding loop.
				 */
				DBG(" YIELD@"PRIvk"\n", ARGvk(that));
				vk_ready(that);
			} 

			/* op is blocked, run next in queue */
            that = vk_proc_dequeue_run(proc_ptr);
		} while (that != NULL);
        if (that == NULL) {
            break;
        }
	}

	rc = vk_heap_exit(vk_proc_get_heap(proc_ptr));
	if (rc == -1) {
		return -1;
	}

	return 0;
}