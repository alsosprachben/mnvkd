#ifndef VK_STATE_H
#define VK_STATE_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <poll.h>
#include <sys/queue.h>

#include "debug.h"
#include "vk_proc.h"
#include "vk_heap.h"
#include "vk_socket.h"
#include "vk_poll.h"
#include "vk_pipe.h"
#include "vk_future.h"

struct vk_thread;

enum VK_PROC_STAT {
	VK_PROC_RUN = 0, /* This coroutine needs to run at the start of its run queue. */
	VK_PROC_YIELD,   /* This coroutine needs to run at the end   of its run queue. */
	VK_PROC_LISTEN,  /* This coroutine is waiting for a vk_request(). */
	VK_PROC_WAIT,    /* This coroutine is waiting for I/O. */
	VK_PROC_ERR,     /* This coroutine needs to run, jumping to the vk_finally(). */
	VK_PROC_END,     /* This coroutine has ended. */
};
void vk_that_clear(                     struct vk_thread *that);
void vk_init(                           struct vk_thread *that, struct vk_proc *proc_ptr, void (*func)(struct vk_thread *that), struct vk_pipe *rx_fd, struct vk_pipe *tx_fd, const char *func_name, char *file, size_t line);
void vk_init_fds(                       struct vk_thread *that, struct vk_proc *proc_ptr, void (*func)(struct vk_thread *that), int rx_fd_arg, int tx_fd_arg,                 const char *func_name, char *file, size_t line);
void vk_init_child(struct vk_thread *parent, struct vk_thread *that,                           void (*func)(struct vk_thread *that),                                               const char *func_name, char *file, size_t line);

int vk_deinit(struct vk_thread *that);

size_t vk_alloc_size();
typedef void (*vk_func)(struct vk_thread *that);
vk_func vk_get_func(struct vk_thread *that);
void vk_set_func(struct vk_thread *that, void (*)(struct vk_thread *that));
const char *vk_get_func_name(struct vk_thread *that);
void vk_set_func_name(struct vk_thread *that, const char *func_name);
char *vk_get_file(struct vk_thread *that);
void vk_set_file(struct vk_thread *that, char *file);
int vk_get_line(struct vk_thread *that);
void vk_set_line(struct vk_thread *that, int line);
int vk_get_counter(struct vk_thread *that);
void vk_set_counter(struct vk_thread *that, int counter);
enum VK_PROC_STAT vk_get_status(struct vk_thread *that);
void vk_set_status(struct vk_thread *that, enum VK_PROC_STAT status);
int vk_get_error(struct vk_thread *that);
void vk_set_error(struct vk_thread *that, int error);
int vk_get_error_counter(struct vk_thread *that);
void vk_set_error_counter(struct vk_thread *that, int error_counter);
struct vk_proc *vk_get_proc(struct vk_thread *that);
void vk_set_proc(struct vk_thread *that, struct vk_proc *proc_ptr);
void *vk_get_self(struct vk_thread *that);
void vk_set_self(struct vk_thread *that, void *self);
struct vk_socket *vk_get_socket(struct vk_thread *that);
void vk_set_socket(struct vk_thread *that, struct vk_socket *socket_ptr);
struct vk_socket *vk_get_waiting_socket(struct vk_thread *that);
void vk_set_waiting_socket(struct vk_thread *that, struct vk_socket *waiting_socket_ptr);
struct vk_future *vk_get_future(struct vk_thread *that);
void vk_set_future(struct vk_thread *that, struct vk_future *ft_ptr);
struct vk_pipe *vk_get_rx_fd(struct vk_thread *that);
void vk_set_rx_fd(struct vk_thread *that, struct vk_pipe *rx_fd);
struct vk_pipe *vk_get_tx_fd(struct vk_thread *that);
void vk_set_tx_fd(struct vk_thread *that, struct vk_pipe *tx_fd);
void vk_enqueue_run(struct vk_thread *that);
int vk_get_enqueued_run(struct vk_thread *that);
void vk_set_enqueued_run(struct vk_thread *that, int run_enq);

/* next coroutine in the proc run queue */
struct vk_thread *vk_next_run_vk(struct vk_thread *that);

/* whether coroutine status is VK_PROC_END */
int vk_is_completed(struct vk_thread *that);

/* whether coroutine status is VK_PROC_RUN or VK_PROC_ERR */
int vk_is_ready(struct vk_thread *that);

/* whether coroutine status is VK_PROC_YIELD:
	a temporary status for enqueued coroutines to set back to VK_PROC_RUN after breaking out of the ready loop.
*/
int vk_is_yielding(struct vk_thread *that);

/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct vk_thread *that);

ssize_t vk_unblock(struct vk_thread *that);

void vk_deblock_waiting_socket(struct vk_thread *that);
void vk_deblock_socket(struct vk_thread *that);
void vk_derun(struct vk_thread *that);

int vk_copy_arg(struct vk_thread *that, void *src, size_t n);

/* primary coroutine */
#define VK_INIT(that, proc_ptr, vk_func, rx_fd_arg, tx_fd_arg) \
	vk_init_fds(that, proc_ptr, vk_func, rx_fd_arg, tx_fd_arg, #vk_func, __FILE__, __LINE__)

#define VK_INIT_CHILD(parent, that, vk_func) \
	vk_init_child(parent, that, vk_func, #vk_func, __FILE__, __LINE__)

/* coroutine-scoped for responder */
#define vk_child(child, vk_func) VK_INIT_CHILD(that, child, vk_func)

/* coroutine-scoped for accepted socket into new heap */
#define vk_accepted(there, vk_func, rx_fd_arg, tx_fd_arg) VK_INIT(there, that->proc_ptr, vk_func, rx_fd_arg, tx_fd_arg)

/* set up pipeline with parent */
/* child coroutine that takes over writes from the parent, connecting via internal pipe the parent's writes to the child's reads */
/* lifecycle: parent read FD -> parent write pipe -> child read pipe -> child write FD */
/* This allows the responder to start writing before the reading is complete. */
#define vk_pipeline(parent) do { \
	/* - bind parent tx to rx */ \
	vk_pipe_init_rx(vk_socket_get_tx_fd(vk_get_socket(parent)), vk_get_socket(that)); \
	/* - bind rx to parent tx */ \
	vk_pipe_init_tx(vk_socket_get_rx_fd(vk_get_socket(that)), vk_get_socket(parent)); \
} while (0)

#define vk_begin_pipeline(parent_ft_ptr) \
	vk_begin(); \
	vk_get_request(parent_ft_ptr); \
	vk_pipeline((parent_ft_ptr)->vk); \
	vk_future_resolve(parent_ft_ptr, 0); \
	vk_respond(parent_ft_ptr)

#define vk_depipeline(parent) do { \
	 \
} while (0)

#include "vk_thread_cr.h"
#include "vk_thread_mem.h"
#include "vk_thread_exec.h"
#include "vk_thread_ft.h"
#include "vk_thread_err.h"
#include "vk_thread_io.h"

#endif
