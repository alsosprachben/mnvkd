#ifndef VK_PROC_H
#define VK_PROC_H

#include <signal.h>
#include <sys/types.h>

struct vk_kern;
struct vk_socket;
struct vk_proc;
struct vk_thread;
struct vk_pool;
struct vk_proc_local;
struct vk_fd_table;

void vk_proc_init(struct vk_proc* proc_ptr);
size_t vk_proc_get_id(struct vk_proc* proc_ptr);
void vk_proc_set_id(struct vk_proc* proc_ptr, size_t proc_id);
size_t vk_proc_get_pool_entry_id(struct vk_proc* proc_ptr);
void vk_proc_set_pool_entry_id(struct vk_proc* proc_ptr, size_t pool_entry_id);
struct vk_pool* vk_proc_get_pool(struct vk_proc* proc_ptr);
void vk_proc_set_pool(struct vk_proc* proc_ptr, struct vk_pool* pool_ptr);
struct vk_pool_entry* vk_proc_get_entry(struct vk_proc* proc_ptr);
void vk_proc_set_entry(struct vk_proc* proc_ptr, struct vk_pool_entry* entry_ptr);

int vk_proc_get_run(struct vk_proc* proc_ptr);
void vk_proc_set_run(struct vk_proc* proc_ptr, int run);
int vk_proc_get_blocked(struct vk_proc* proc_ptr);
void vk_proc_set_blocked(struct vk_proc* proc_ptr, int blocked);
int vk_proc_is_zombie(struct vk_proc* proc_ptr);

struct vk_fd* vk_proc_first_fd(struct vk_proc* proc_ptr);
int vk_proc_allocate_fd(struct vk_proc* proc_ptr, struct vk_fd* fd_ptr, int fd);
void vk_proc_deallocate_fd(struct vk_proc* proc_ptr, struct vk_fd* fd_ptr);

int vk_proc_get_privileged(struct vk_proc* proc_ptr);
void vk_proc_set_privileged(struct vk_proc* proc_ptr, int privileged);
int vk_proc_get_isolated(struct vk_proc* proc_ptr);
void vk_proc_set_isolated(struct vk_proc* proc_ptr, int isolated);

void vk_proc_clear(struct vk_proc* proc_ptr);
int vk_proc_alloc(struct vk_proc* proc_ptr, void* map_addr, size_t map_len, int map_prot, int map_flags, int map_fd,
		  off_t map_offset, int entered, int collapse);
int vk_proc_free(struct vk_proc* proc_ptr);
int vk_proc_alloc_from_pool(struct vk_proc* proc_ptr, struct vk_pool* pool_ptr);
int vk_proc_free_from_pool(struct vk_proc* proc_ptr, struct vk_pool* pool_ptr);

struct vk_thread* vk_proc_alloc_thread(struct vk_proc* proc_ptr);
int vk_proc_free_thread(struct vk_proc* proc_ptr);

size_t vk_proc_alloc_size();

struct vk_heap* vk_proc_get_heap(struct vk_proc* proc_ptr);

/* next proc in the kernel run queue */
struct vk_proc* vk_proc_next_run_proc(struct vk_proc* proc_ptr);

/* next proc in the kernel blocked queue */
struct vk_proc* vk_proc_next_blocked_proc(struct vk_proc* proc_ptr);

int vk_proc_prepoll(struct vk_proc* proc_ptr, struct vk_fd_table* fd_table_ptr);
int vk_proc_postpoll(struct vk_proc* proc_ptr, struct vk_fd_table* fd_table_ptr);

/* execute until the run queue is drained */
int vk_proc_execute(struct vk_proc* proc_ptr, struct vk_kern* kern_ptr);

#define PRproc "<proc       id=\"%4zu\" active=\"%c\" blocked=\"%c\">"
#define ARGproc(proc_ptr)                                                                                              \
	vk_proc_get_id(proc_ptr), (vk_proc_get_run(proc_ptr) ? 't' : 'f'), (vk_proc_get_blocked(proc_ptr) ? 't' : 'f')

#define vk_proc_logf(fmt, ...) ERR("      proc: " PRloc " " PRproc ": " fmt, ARGloc, ARGproc(proc_ptr), __VA_ARGS__)
#define vk_proc_dbgf(fmt, ...) DBG("      proc: " PRloc " " PRproc ": " fmt, ARGloc, ARGproc(proc_ptr), __VA_ARGS__)
#define vk_proc_log(note)                                                                                              \
	ERR("      proc: " PRloc " " PRproc ": "                                                                       \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGproc(proc_ptr), note)
#define vk_proc_dbg(note)                                                                                              \
	DBG("      proc: " PRloc " " PRproc ": "                                                                       \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGproc(proc_ptr), note)
#define vk_proc_perror(string) vk_proc_logf("%s: %s\n", string, strerror(errno))

#define VK_PROC_INIT_PUBLIC(proc_ptr, map_len, entered, collapse)                                                                \
	vk_proc_alloc(proc_ptr, NULL, map_len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0, entered, collapse)
#define VK_PROC_INIT_PRIVATE(proc_ptr, map_len, entered, collapse)                                                               \
	vk_proc_alloc(proc_ptr, NULL, map_len, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0, entered, collapse)

#endif
