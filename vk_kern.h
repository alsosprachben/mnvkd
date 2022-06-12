#ifndef VK_KERN_H
#define VK_KERN_H

#include "vk_heap.h"
#include "vk_proc.h"

#include <sys/types.h>

struct vk_kern;
void vk_kern_clear(struct vk_kern *kern_ptr);
struct vk_kern *vk_kern_alloc(struct vk_heap_descriptor *hd_ptr);
size_t vk_kern_alloc_size();
struct vk_proc *vk_kern_alloc_proc(struct vk_kern *kern_ptr);
void vk_kern_free_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);

void vk_kern_enqueue_run(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);
void vk_kern_enqueue_blocked(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);
void vk_kern_drop_run(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);
void vk_kern_drop_blocked(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);
struct vk_proc *vk_kern_dequeue_run(struct vk_kern *kern_ptr);
struct vk_proc *vk_kern_dequeue_blocked(struct vk_kern *kern_ptr);
void vk_kern_flush_proc_queues(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);
int vk_kern_pending(struct vk_kern *kern_ptr);
int vk_kern_prepoll_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);
int vk_kern_prepoll(struct vk_kern *kern_ptr);
int vk_kern_postpoll(struct vk_kern *kern_ptr);
int vk_kern_poll(struct vk_kern *kern_ptr);

#endif