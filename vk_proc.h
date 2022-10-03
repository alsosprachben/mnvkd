#ifndef VK_PROC_H
#define VK_PROC_H

#include <sys/types.h>
#include <signal.h>

struct vk_socket;
struct vk_proc;
struct vk_thread;
#define VK_PROC_MAX_EVENTS 16

void vk_proc_clear(struct vk_proc *proc_ptr);
int vk_proc_init(struct vk_proc *proc_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset);
int vk_proc_deinit(struct vk_proc *proc_ptr);

struct vk_thread *vk_proc_alloc_that(struct vk_proc *proc_ptr);
int vk_proc_free_that(struct vk_proc *proc_ptr);

size_t vk_proc_alloc_size();

struct vk_heap *vk_proc_get_heap(struct vk_proc *proc_ptr);

/* next proc in the kernel run queue */
struct vk_proc *vk_proc_next_run_proc(struct vk_proc *proc_ptr);

/* next proc in the kernel blocked queue */
struct vk_proc *vk_proc_next_blocked_proc(struct vk_proc *proc_ptr);

/* first coroutine in the proc run queue */
struct vk_thread *vk_proc_first_run(struct vk_proc *proc_ptr);

/* first socket in the proc blocked queue */
struct vk_socket *vk_proc_first_blocked(struct vk_proc *proc_ptr);

/* get running CR, or NULL, for exception handling */
struct vk_thread *vk_proc_get_running(struct vk_proc *proc_ptr);

/* get supervisor CR, or NULL, for exception handling */
struct vk_thread *vk_proc_get_supervisor(struct vk_proc *proc_ptr);

/* set supervisor CR, or NULL to disable -- if a supervisor is set, signals are delivered to it instead of the running thread */
void vk_proc_set_supervisor(struct vk_proc *proc_ptr, struct vk_thread *that);

/* get signal info, for exception handling */
siginfo_t *vk_proc_get_siginfo(struct vk_proc *proc_ptr);

/* set sinfo info, for exception handling */
void vk_proc_set_siginfo(struct vk_proc *proc_ptr, siginfo_t siginfo);

/* get execution context, for exception handling */
ucontext_t *vk_proc_get_uc(struct vk_proc *proc_ptr);

/* set execution context, for exception handling */
void vk_proc_set_uc(struct vk_proc *proc_ptr, ucontext_t *uc_ptr);

/* clear siginfo and uc, to reset exception state */
void vk_proc_clear_signal(struct vk_proc *proc_ptr);

/* process has no coroutine in run queue nor blocked queue */
int vk_proc_is_zombie(struct vk_proc *proc_ptr);

/* enqueue coroutine to run queue */
void vk_proc_enqueue_run(struct vk_proc *proc_ptr, struct vk_thread *that);

/* dequeue coroutine from run queue, or NULL if empty */
struct vk_thread *vk_proc_dequeue_run(struct vk_proc *proc_ptr);

/* enqueue socket to blocked queue */
void vk_proc_enqueue_blocked(struct vk_proc *proc_ptr, struct vk_socket *socket_ptr);

/* drop coroutine from run queue */
void vk_proc_drop_run(struct vk_proc *proc_ptr, struct vk_thread *that);

/* drop socket from blocked queue */
void vk_proc_drop_blocked(struct vk_proc *proc_ptr, struct vk_socket *socket_ptr);

/* dequeue socket from blocked queue, or NULL if empty */
struct vk_socket *vk_proc_dequeue_blocked(struct vk_proc *proc_ptr);

struct vk_heap *vk_proc_get_heap(struct vk_proc *proc_ptr);

struct vk_kern *vk_proc_get_kern(struct vk_proc *proc_ptr);

/* execute until the run queue is drained */
int vk_proc_execute(struct vk_proc *proc_ptr);

/* add blocked coroutines to the poll events */
int vk_proc_prepoll(struct vk_proc *proc_ptr);

/* wait for poll events */
int vk_proc_poll(struct vk_proc *proc_ptr);

/* add the coroutines of unblocked sockets to the run queue */
int vk_proc_postpoll(struct vk_proc *proc_ptr);

#define VK_PROC_INIT_PUBLIC(proc_ptr, map_len) vk_proc_init(proc_ptr, NULL, map_len, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0)
#define VK_PROC_INIT_PRIVATE( proc_ptr, map_len) vk_proc_init(proc_ptr, NULL, map_len, PROT_NONE,            MAP_ANON|MAP_PRIVATE, -1, 0)

#endif