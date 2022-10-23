#ifndef VK_HEAP_H
#define VK_HEAP_H

#include <sys/types.h>

struct vk_heap;

size_t vk_heap_alloc_size();

int vk_heap_map(struct vk_heap *hd, void *addr, size_t len, int prot, int flags, int fd, off_t offset, int entered);
int vk_heap_unmap(struct vk_heap *hd);
int vk_heap_enter(struct vk_heap *hd);
int vk_heap_exit(struct vk_heap *hd);

void *vk_heap_push(struct vk_heap *hd, size_t nmemb, size_t count);
int vk_heap_pop(struct vk_heap *hd);

void *vk_heap_get_start(struct vk_heap *hd);
void *vk_heap_get_cursor(struct vk_heap *hd);
void *vk_heap_get_stop(struct vk_heap *hd);
size_t vk_heap_get_free(struct vk_heap *hd);

#endif
