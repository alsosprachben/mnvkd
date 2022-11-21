#ifndef VK_PROC_H
#define VK_PROC_H

#include <sys/types.h>
#include <signal.h>

struct vk_socket;
struct vk_proc;
struct vk_thread;
struct vk_pool;
struct vk_proc_local;
#define VK_PROC_MAX_EVENTS 16

void vk_proc_init(struct vk_proc *proc_ptr);
void vk_proc_clear(struct vk_proc *proc_ptr);
int vk_proc_alloc(struct vk_proc *proc_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset, int entered);
int vk_proc_free(struct vk_proc *proc_ptr);
int vk_proc_alloc_from_pool(struct vk_proc *proc_ptr, struct vk_pool *pool_ptr);
int vk_proc_free_from_pool(struct vk_proc *proc_ptr, struct vk_pool *pool_ptr);

struct vk_thread *vk_proc_alloc_thread(struct vk_proc *proc_ptr);
int vk_proc_free_thread(struct vk_proc *proc_ptr);

size_t vk_proc_alloc_size();

struct vk_heap *vk_proc_get_heap(struct vk_proc *proc_ptr);

/* next proc in the kernel run queue */
struct vk_proc *vk_proc_next_run_proc(struct vk_proc *proc_ptr);

/* next proc in the kernel blocked queue */
struct vk_proc *vk_proc_next_blocked_proc(struct vk_proc *proc_ptr);


struct vk_heap *vk_proc_get_heap(struct vk_proc *proc_ptr);

/* execute until the run queue is drained */
int vk_proc_execute(struct vk_proc *proc_ptr);

/* add blocked coroutines to the poll events */
int vk_proc_prepoll(struct vk_proc *proc_ptr);

/* wait for poll events */
int vk_proc_poll(struct vk_proc *proc_ptr);

/* add the coroutines of unblocked sockets to the run queue */
int vk_proc_postpoll(struct vk_proc *proc_ptr);

#define VK_PROC_INIT_PUBLIC(proc_ptr, map_len, entered) vk_proc_alloc(proc_ptr, NULL, map_len, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0, entered)
#define VK_PROC_INIT_PRIVATE( proc_ptr, map_len, entered) vk_proc_alloc(proc_ptr, NULL, map_len, PROT_NONE,            MAP_ANON|MAP_PRIVATE, -1, 0, entered)

#endif
