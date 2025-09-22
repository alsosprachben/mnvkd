#ifndef VK_KERN_H
#define VK_KERN_H

struct vk_proc;
struct vk_heap;
struct vk_pool;

#include <signal.h>
#include <sys/types.h>

struct vk_kern;
#include "vk_isolate.h"
#include "vk_proc_local.h"

struct vk_kern_mainline_udata;
void vk_kern_mainline_udata_set_kern(struct vk_kern_mainline_udata *udata, struct vk_kern *kern_ptr);
struct vk_kern *vk_kern_mainline_udata_get_kern(struct vk_kern_mainline_udata *udata);
void vk_kern_mainline_udata_set_proc(struct vk_kern_mainline_udata *udata, struct vk_proc *proc_ptr);
struct vk_proc *vk_kern_mainline_udata_get_proc(struct vk_kern_mainline_udata *udata);
void vk_kern_mainline_udata_save_kern_hd(struct vk_kern_mainline_udata *udata);
void vk_kern_mainline_udata_open_kern_hd(struct vk_kern_mainline_udata *udata);
struct vk_heap *vk_kern_mainline_udata_get_kern_hd(struct vk_kern_mainline_udata *udata);

void vk_kern_clear(struct vk_kern* kern_ptr);
void vk_kern_set_shutdown_requested(struct vk_kern* kern_ptr);
void vk_kern_clear_shutdown_requested(struct vk_kern* kern_ptr);
int vk_kern_get_shutdown_requested(struct vk_kern* kern_ptr);

struct vk_heap* vk_kern_get_heap(struct vk_kern* kern_ptr);

struct vk_fd_table* vk_kern_get_fd_table(struct vk_kern* kern_ptr);

struct vk_proc* vk_kern_get_proc(struct vk_kern* kern_ptr, size_t i);

struct vk_kern* vk_kern_alloc(struct vk_heap* hd_ptr);
size_t vk_kern_alloc_size();
struct vk_proc* vk_kern_alloc_proc(struct vk_kern* kern_ptr, struct vk_pool* pool_ptr);
void vk_kern_free_proc(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);

struct vk_proc* vk_kern_first_run(struct vk_kern* kern_ptr);
struct vk_proc* vk_kern_first_blocked(struct vk_kern* kern_ptr);
struct vk_proc* vk_kern_first_deferred(struct vk_kern* kern_ptr);
void vk_kern_enqueue_run(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
void vk_kern_enqueue_blocked(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
void vk_kern_enqueue_deferred(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
void vk_kern_drop_run(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
void vk_kern_drop_blocked(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
void vk_kern_drop_deferred(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
struct vk_proc* vk_kern_dequeue_run(struct vk_kern* kern_ptr);
struct vk_proc* vk_kern_dequeue_blocked(struct vk_kern* kern_ptr);
struct vk_proc* vk_kern_dequeue_deferred(struct vk_kern* kern_ptr);
void vk_kern_flush_proc_queues(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
int vk_kern_pending(struct vk_kern* kern_ptr);
int vk_kern_prepoll(struct vk_kern* kern_ptr);
int vk_kern_postpoll(struct vk_kern* kern_ptr);
int vk_kern_poll(struct vk_kern* kern_ptr);
void vk_proc_execute_mainline(void* mainline_udata);
void vk_proc_execute_jumper(void* jumper_udata, siginfo_t* siginfo_ptr, ucontext_t* uc_ptr);
int vk_kern_dispatch_proc(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr);
int vk_kern_loop(struct vk_kern* kern_ptr);

/* isolate helpers */
vk_isolate_t *vk_kern_get_isolate(struct vk_kern* kern_ptr);
void vk_kern_set_isolate_user_state(struct vk_kern* kern_ptr, struct vk_proc_local* pl);

void vk_kern_sync_signal_handler(struct vk_kern* kern_ptr, int signo);
void vk_kern_receive_signal(struct vk_kern* kern_ptr);
void vk_kern_signal_handler(void* handler_udata, int jump, siginfo_t* siginfo_ptr, ucontext_t* uc_ptr);
void vk_kern_signal_jumper(void* handler_udata, siginfo_t* siginfo_ptr, ucontext_t* uc_ptr);

#endif
