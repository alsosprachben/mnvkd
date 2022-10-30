#include <errno.h>

#include "vk_pool_s.h"
#include "vk_heap.h"

int vk_object_init_func_noop(void *object_ptr, void *udata) {
    return 0;
}

struct vk_pool_entry *vk_pool_get(struct vk_pool *pool_ptr, size_t i) {
    if (i >= pool_ptr->object_count) {
        return NULL;
    }

    return &(((struct vk_pool_entry *) vk_heap_get_start(&pool_ptr->pool_heap))[i]);
}

int vk_pool_free(struct vk_pool *pool_ptr, struct vk_pool_entry *entry_ptr) {
    if (entry_ptr == NULL) {
        errno = ENOENT;
        return -1;
    }
    SLIST_INSERT_HEAD(&pool_ptr->free_entries, entry_ptr, free_list_elem);
    return 0;
}

int vk_pool_init(struct vk_pool *pool_ptr, size_t object_size, size_t object_count, int object_contiguity, vk_object_init_func object_init_func, void *udata) {
    int rc;
    size_t i;

    pool_ptr->object_size = object_size;
    pool_ptr->object_count = object_count;
    pool_ptr->object_contiguity = object_contiguity;
    pool_ptr->object_init_func = object_init_func;
    pool_ptr->object_init_func_udata = udata;
    if (pool_ptr->object_init_func == NULL) {
        pool_ptr->object_init_func = vk_object_init_func_noop;
    }

    SLIST_INIT(&pool_ptr->free_entries);

    rc = vk_heap_map(&pool_ptr->pool_heap, NULL, vk_pagesize() * vk_blocklen(pool_ptr->object_count, sizeof (struct vk_pool_entry)), 0, MAP_ANON|MAP_PRIVATE, -1, 0, 1);
    if (rc == -1) {
        return -1;
    }

    for (i = 0; i < pool_ptr->object_count; i++) {
        struct vk_pool_entry *entry_ptr;
        entry_ptr = vk_pool_get(pool_ptr, i);

        rc = vk_pool_free(pool_ptr, entry_ptr);
        if (rc == -1) {
            return -1;
        }

        rc = pool_ptr->object_init_func(vk_heap_get_start(&entry_ptr->heap), pool_ptr->object_init_func_udata);
        if (rc == -1) {
            return -1;
        }
    }

    return 0;
}

int vk_pool_deinit(struct vk_pool *pool_ptr) {
    int rc;

    rc = vk_heap_unmap(&pool_ptr->pool_heap);
    if (rc == -1) {
        return -1;
    }

    return 0;
}