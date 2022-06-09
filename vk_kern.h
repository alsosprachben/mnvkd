#ifndef VK_KERN_H
#define VK_KERN_H

#include "vk_heap.h"
#include "vk_proc.h"

#include <sys/types.h>

struct vk_kern;
void vk_kern_clear(struct vk_kern *kern_ptr);
void vk_kern_init(struct vk_kern *kern_ptr, struct vk_heap_descriptor *hd_ptr);
size_t vk_kern_alloc_size();
struct vk_proc *vk_kern_alloc_proc(struct vk_kern *kern_ptr);
void vk_kern_free_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);

#endif