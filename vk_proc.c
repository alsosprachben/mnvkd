#include "vk_proc.h"
#include "vk_proc_s.h"

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