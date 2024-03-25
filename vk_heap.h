#ifndef VK_HEAP_H
#define VK_HEAP_H

#include <sys/types.h>
#include <sys/mman.h>

struct vk_heap;

size_t vk_heap_alloc_size();

int vk_heap_map(struct vk_heap *hd, void *addr, size_t len, int prot, int flags, int fd, off_t offset, int entered);
int vk_heap_buf(struct vk_heap *hd, void *addr, size_t len, int prot, int flags, int fd, off_t offset, int entered);
int vk_heap_unmap(struct vk_heap *hd);
int vk_heap_owned(struct vk_heap *hd);
int vk_heap_entered(struct vk_heap *hd);
int vk_heap_enter(struct vk_heap *hd);
int vk_heap_exit(struct vk_heap *hd);

struct vk_stack *vk_heap_get_stack(struct vk_heap *hd);

#define PRheap "<heap" \
    " owned=\"%c\"" \
    " entered=\"%c\"" \
">"
#define ARGheap(heap_ptr) \
    vk_heap_owned(heap_ptr) ? 't' : 'f', \
    vk_heap_entered(heap_ptr) ? 't' : 'f' \

#endif
