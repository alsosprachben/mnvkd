#ifndef VK_POOL_H
#define VK_POOL_H

#include <stddef.h>

#include "vk_heap.h"

struct vk_pool_entry;
struct vk_pool;
typedef int (*vk_object_init_func)(void *object_ptr, void *udata);

size_t vk_pool_entry_get_id(struct vk_pool_entry *entry_ptr);
struct vk_heap *vk_pool_entry_get_heap(struct vk_pool_entry *entry_ptr);

struct vk_pool_entry *vk_pool_get_entry(struct vk_pool *pool_ptr, size_t i);
struct vk_pool_entry *vk_pool_alloc_entry(struct vk_pool *pool_ptr);
int vk_pool_free_entry(struct vk_pool *pool_ptr, struct vk_pool_entry *entry_ptr);
int vk_pool_init(struct vk_pool *pool_ptr, size_t object_size, size_t object_count, int object_contiguity, vk_object_init_func object_init_func, void *object_init_func_udata, vk_object_init_func object_deinit_func, void *object_deinit_func_udata, int entered);
int vk_pool_deinit(struct vk_pool *pool_ptr);


#endif