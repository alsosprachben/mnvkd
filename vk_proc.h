#ifndef VK_PROC_H
#define VK_PROC_H

#include <sys/types.h>

struct vk_socket;
struct vk_proc;
struct that;
#define VK_PROC_MAX_EVENTS 16

int vk_proc_init(struct vk_proc *proc_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset);
int vk_proc_deinit(struct vk_proc *proc_ptr);

size_t vk_proc_alloc_size();

/* coroutine is waiting in the run queue */
int vk_proc_pending(struct vk_proc *proc_ptr);

/* enqueue coroutine to run queue */
void vk_proc_enqueue_run(struct vk_proc *proc_ptr, struct that *that);

/* dequeue coroutine from run queue, or NULL if empty */
struct that *vk_proc_dequeue_run(struct vk_proc *proc_ptr);

/* enqueue coroutine to blocked queue */
void vk_proc_enqueue_blocked(struct vk_proc *proc_ptr, struct vk_socket *socket_ptr);

/* dequeue coroutine from blocked queue, or NULL if empty */
struct vk_socket *vk_proc_dequeue_blocked(struct vk_proc *proc_ptr);

struct vk_heap_descriptor *vk_proc_get_heap(struct vk_proc *proc_ptr);

int vk_proc_execute(struct vk_proc *proc_ptr);
int vk_proc_poll(struct vk_proc *proc_ptr);

#define VK_PROC_INIT_PRIVATE(proc_ptr, map_len) vk_proc_init(proc_ptr, NULL, map_len, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0)
#define VK_PROC_INIT_PUBLIC( proc_ptr, map_len) vk_proc_init(proc_ptr, NULL, map_len, PROT_NONE,            MAP_ANON|MAP_PRIVATE, -1, 0)

#endif