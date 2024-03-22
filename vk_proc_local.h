#ifndef VK_PROC_LOCAL_H
#define VK_PROC_LOCAL_H

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

struct vk_proc;
struct vk_proc_local;
struct vk_fd_table;
void vk_proc_local_init(struct vk_proc_local *proc_local_ptr);
size_t vk_proc_local_alloc_size();

size_t vk_proc_local_get_proc_id(struct vk_proc_local *proc_local_ptr);
void vk_proc_local_set_proc_id(struct vk_proc_local *proc_local_ptr, size_t proc_id);
struct vk_stack *vk_proc_local_get_stack(struct vk_proc_local *proc_local_ptr);
void vk_proc_local_set_stack(struct vk_proc_local *proc_local_ptr, struct vk_stack *stack_ptr);

int vk_proc_local_get_run(struct vk_proc_local *proc_local_ptr);
void vk_proc_local_set_run(struct vk_proc_local *proc_local_ptr, int run);
int vk_proc_local_get_blocked(struct vk_proc_local *proc_local_ptr);
void vk_proc_local_set_blocked(struct vk_proc_local *proc_local_ptr, int blocked);

struct vk_proc_local *vk_proc_get_local(struct vk_proc *proc_ptr);

/* first coroutine in the proc run queue */
struct vk_thread *vk_proc_local_first_run(struct vk_proc_local *proc_local_ptr);

/* first socket in the proc blocked queue */
struct vk_socket *vk_proc_local_first_blocked(struct vk_proc_local *proc_local_ptr);

/* get running CR, or NULL, for exception handling */
struct vk_thread *vk_proc_local_get_running(struct vk_proc_local *proc_local_ptr);

/* set running CR, for exception handling */
void vk_proc_local_set_running(struct vk_proc_local *proc_local_ptr, struct vk_thread *that);

/* get supervisor CR, or NULL, for exception handling */
struct vk_thread *vk_proc_local_get_supervisor(struct vk_proc_local *proc_local_ptr);

/* set supervisor CR, or NULL to disable -- if a supervisor is set, signals are delivered to it instead of the running thread */
void vk_proc_local_set_supervisor(struct vk_proc_local *proc_local_ptr, struct vk_thread *that);

/* get signal info, for exception handling */
siginfo_t *vk_proc_local_get_siginfo(struct vk_proc_local *proc_local_ptr);

/* set sinfo info, for exception handling */
void vk_proc_local_set_siginfo(struct vk_proc_local *proc_local_ptr, siginfo_t siginfo);

/* get execution context, for exception handling */
ucontext_t *vk_proc_local_get_uc(struct vk_proc_local *proc_local_ptr);

/* set execution context, for exception handling */
void vk_proc_local_set_uc(struct vk_proc_local *proc_local_ptr, ucontext_t *uc_ptr);

/* clear siginfo and uc, to reset exception state */
void vk_proc_local_clear_signal(struct vk_proc_local *proc_local_ptr);

/* process has no coroutine in run queue nor blocked queue */
int vk_proc_local_is_zombie(struct vk_proc_local *proc_local_ptr);

/* enqueue coroutine to run queue */
void vk_proc_local_enqueue_run(struct vk_proc_local *proc_local_ptr, struct vk_thread *that);

/* dequeue coroutine from run queue, or NULL if empty */
struct vk_thread *vk_proc_local_dequeue_run(struct vk_proc_local *proc_local_ptr);

/* enqueue socket to blocked queue */
void vk_proc_local_enqueue_blocked(struct vk_proc_local *proc_local_ptr, struct vk_socket *socket_ptr);

/* drop coroutine from run queue */
void vk_proc_local_drop_run(struct vk_proc_local *proc_local_ptr, struct vk_thread *that);

/* drop sockets from blocked queue referenced by thread */
void vk_proc_local_drop_blocked_for(struct vk_proc_local *proc_local_ptr, struct vk_thread *that);

/* drop socket from blocked queue */
void vk_proc_local_drop_blocked(struct vk_proc_local *proc_local_ptr, struct vk_socket *socket_ptr);

/* dequeue socket from blocked queue, or NULL if empty */
struct vk_socket *vk_proc_local_dequeue_blocked(struct vk_proc_local *proc_local_ptr);

void vk_proc_local_dump_run_q(struct vk_proc_local *proc_local_ptr);
void vk_proc_local_dump_blocked_q(struct vk_proc_local *proc_local_ptr);

int vk_proc_local_retry_socket(struct vk_proc_local *proc_local_ptr, struct vk_socket *socket_ptr);

int vk_proc_local_raise_signal(struct vk_proc_local *proc_local_ptr);

int vk_proc_local_execute(struct vk_proc_local *proc_local_ptr);

#endif