#ifndef VK_HEAP_H
#define VK_HEAP_H

struct vk_heap_descriptor;

int vk_heap_map(struct vk_heap_descriptor *hd, void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int vk_heap_unmap(struct vk_heap_descriptor *hd);
int vk_heap_enter(struct vk_heap_descriptor *hd);
int vk_heap_exit(struct vk_heap_descriptor *hd);

void *vk_heap_push(struct vk_heap_descriptor *hd, size_t nmemb, size_t count);
int vk_heap_pop(struct vk_heap_descriptor *hd);

#endif
