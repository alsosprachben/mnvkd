#ifndef VK_KERN_H
#define VK_KERN_H

#include "vk_heap.h"
#include "vk_proc.h"

#include <sys/types.h>

struct vk_kern;
void vk_kern_clear(struct vk_kern *kern_ptr);
void vk_kern_set_shutdown_requested(struct vk_kern *kern_ptr);
void vk_kern_clear_shutdown_requested(struct vk_kern *kern_ptr);
int vk_kern_get_shutdown_requested(struct vk_kern *kern_ptr);

struct vk_proc *vk_kern_get_proc(struct vk_kern *kern_ptr, size_t i);

struct vk_kern *vk_kern_alloc(struct vk_heap *hd_ptr);
size_t vk_kern_alloc_size();
struct vk_proc *vk_kern_alloc_proc(struct vk_kern *kern_ptr, struct vk_pool *pool_ptr);
void vk_kern_free_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);

struct vk_proc *vk_kern_first_run(struct vk_kern *kern_ptr);
struct vk_proc *vk_kern_first_blocked(struct vk_kern *kern_ptr);
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
void vk_proc_execute_mainline(void *mainline_udata);
void vk_proc_execute_jumper(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr);
int vk_kern_dispatch_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr);
int vk_kern_loop(struct vk_kern *kern_ptr);

void vk_kern_signal_jumper(void *handler_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr);

#endif