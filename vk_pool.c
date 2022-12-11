#include <errno.h>

#include "vk_pool_s.h"
#include "vk_heap.h"

#include "vk_debug.h"
#include "vk_wrapguard.h"

size_t vk_pool_alloc_size() {
    return sizeof (struct vk_pool);
}

size_t vk_pool_entry_get_id(struct vk_pool_entry *entry_ptr) {
    return entry_ptr->entry_id;
}

struct vk_heap *vk_pool_entry_get_heap(struct vk_pool_entry *entry_ptr) {
    return &entry_ptr->heap;
}

int vk_object_init_func_noop(struct vk_pool_entry *entry_ptr, void *udata) {
    return 0;
}

struct vk_pool_entry *vk_pool_get_entry(struct vk_pool *pool_ptr, size_t i) {
    if (i >= pool_ptr->object_count) {
        return NULL;
    }

    return &(((struct vk_pool_entry *) vk_stack_get_start(vk_heap_get_stack(&pool_ptr->pool_heap)))[i]);
}

/* Traverse entry list. NULL entry_ptr starts from the beginning. NULL return marks the end. */
struct vk_pool_entry *vk_pool_next_entry(struct vk_pool *pool_ptr, struct vk_pool_entry *entry_ptr) {
    size_t entry_id;
    
    if (entry_ptr == NULL) {
        /* NULL starts from beginning */
        entry_id = 0;
    } else {
        /* not NULL is the next one */
        entry_id = entry_ptr->entry_id + 1;
    }
    return vk_pool_get_entry(pool_ptr, entry_id);
}

struct vk_pool_entry *vk_pool_alloc_entry(struct vk_pool *pool_ptr) {
    struct vk_pool_entry *entry_ptr;

    if (SLIST_EMPTY(&pool_ptr->free_entries)) {
        errno = ENOMEM;
        return NULL;
    }

    entry_ptr = SLIST_FIRST(&pool_ptr->free_entries);
    SLIST_REMOVE_HEAD(&pool_ptr->free_entries, free_list_elem);

    DBG("Allocated Pool Entry ID %zu\n", entry_ptr->entry_id);

    return entry_ptr;
}

int vk_pool_free_entry(struct vk_pool *pool_ptr, struct vk_pool_entry *entry_ptr) {
    int rc;
    if (entry_ptr == NULL) {
        errno = ENOENT;
        return -1;
    }
    SLIST_INSERT_HEAD(&pool_ptr->free_entries, entry_ptr, free_list_elem);

    if (pool_ptr->object_free_func != NULL) {
        rc = pool_ptr->object_free_func(entry_ptr, pool_ptr->object_free_func_udata);
        if (rc == -1) {
            return -1;
        }
    }

    return 0;
}

int vk_pool_init(struct vk_pool *pool_ptr, size_t object_size, size_t object_count, vk_pool_entry_init_func object_init_func, void *object_init_func_udata, vk_pool_entry_init_func object_free_func, void *object_free_func_udata, vk_pool_entry_init_func object_deinit_func, void *object_deinit_func_udata, int entered) {
    size_t alignedlen;
    int rc;
    int i;

    pool_ptr->object_size = object_size;
    pool_ptr->object_count = object_count;
    pool_ptr->object_init_func = object_init_func;
    pool_ptr->object_init_func_udata = object_init_func_udata;
    pool_ptr->object_free_func = object_free_func;
    pool_ptr->object_free_func_udata = object_free_func_udata;
    pool_ptr->object_deinit_func = object_deinit_func;
    pool_ptr->object_deinit_func_udata = object_deinit_func_udata;


    SLIST_INIT(&pool_ptr->free_entries);

    rc = vk_safe_alignedlen(pool_ptr->object_count, sizeof (struct vk_pool_entry), &alignedlen);
    if (rc == -1) {
        return -1;
    }
    rc = vk_heap_map(&pool_ptr->pool_heap, NULL, alignedlen, 0, MAP_ANON|MAP_PRIVATE, -1, 0, 1);
    if (rc == -1) {
        return -1;
    }

    for (i = pool_ptr->object_count - 1; i >= 0; i--) {
        struct vk_pool_entry *entry_ptr;
        entry_ptr = vk_pool_get_entry(pool_ptr, i);
        entry_ptr->entry_id = i;

        rc = vk_heap_map(&entry_ptr->heap, NULL, pool_ptr->object_size, PROT_NONE, MAP_ANON|MAP_PRIVATE, -1, 0, entered);
        if (rc == -1) {
            return -1;
        }

        rc = vk_pool_free_entry(pool_ptr, entry_ptr);
        if (rc == -1) {
            return -1;
        }

        if (pool_ptr->object_init_func != NULL) {
            rc = pool_ptr->object_init_func(entry_ptr, pool_ptr->object_init_func_udata);
            if (rc == -1) {
                return -1;
            }
        }
    }

    return 0;
}

int vk_pool_deinit(struct vk_pool *pool_ptr) {
    int rc;
    int i;

    for (i = 0; i < pool_ptr->object_count; i++) {
        struct vk_pool_entry *entry_ptr;
        entry_ptr = vk_pool_get_entry(pool_ptr, i);

        if (pool_ptr->object_deinit_func != NULL) {
            rc = pool_ptr->object_deinit_func(entry_ptr, pool_ptr->object_deinit_func_udata);
        }

        rc = vk_heap_unmap(&entry_ptr->heap);
        if (rc == -1) {
            return -1;
        }
    }

    rc = vk_heap_unmap(&pool_ptr->pool_heap);
    if (rc == -1) {
        return -1;
    }

    return 0;
}

int vk_pool_entry_deinit_func(void *object_ptr, void *udata) {
    return 0;
}
