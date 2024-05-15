/* Copyright 2022 BCW. All Rights Reserved. */
#include <string.h>

#include "vk_future_s.h"
#include "vk_thread.h"
#include "vk_thread_s.h"

#include "vk_debug.h"
#include "vk_heap.h"
#include "vk_proc_local.h"
#include "vk_stack.h"

void vk_thread_clear(struct vk_thread* that) { memset(that, 0, sizeof(*that)); }

void vk_init(struct vk_thread* that, struct vk_proc_local* proc_local_ptr, void (*func)(struct vk_thread* that),
	     struct vk_pipe* rx_fd, struct vk_pipe* tx_fd, const char* func_name, char* file, size_t line)
{
	that->func = func;
	that->func_name = func_name;
	that->file = file;
	that->line = line;
	that->counter = -1;
	that->status = VK_PROC_RUN;
	that->error = 0;
	that->error_counter = -2;
	that->rx_fd = *rx_fd;
	that->tx_fd = *tx_fd;
	that->proc_local_ptr = proc_local_ptr;
	that->socket_ptr = vk_stack_push(vk_proc_local_get_stack(that->proc_local_ptr), 1, vk_socket_size(that->socket_ptr));
	that->self = vk_stack_get_cursor(vk_proc_local_get_stack(that->proc_local_ptr));
	vk_socket_init(that->socket_ptr, that, vk_get_rx_fd(that), vk_get_tx_fd(that));
	that->waiting_socket_ptr = NULL;
	that->ft_q.tqh_first = NULL;
	that->ft_q.tqh_last = NULL;
	TAILQ_INIT(&that->ft_q);
	that->run_q_elem.sle_next = NULL;
	that->run_enq = 0;
}

void vk_init_fds(struct vk_thread* that, struct vk_proc_local* proc_local_ptr, void (*func)(struct vk_thread* that),
		 int rx_fd_arg, enum vk_fd_type rx_fd_type, int tx_fd_arg, enum vk_fd_type tx_fd_type,
		 const char* func_name, char* file, size_t line)
{
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	vk_pipe_init_fd(&rx_fd, rx_fd_arg, rx_fd_type);
	vk_pipe_init_fd(&tx_fd, tx_fd_arg, tx_fd_type);
	return vk_init(that, proc_local_ptr, func, &rx_fd, &tx_fd, func_name, file, line);
}

/* vertical: in -> {parent,child} -> out -- each child inherents the FDs of the parent */
void vk_init_child(struct vk_thread* parent, struct vk_thread* that, void (*func)(struct vk_thread* that),
		   const char* func_name, char* file, size_t line)
{
	return vk_init(that, parent->proc_local_ptr, func, &parent->rx_fd, &parent->tx_fd, func_name, file, line);
}

/* loop: in -> parent <-> child -> out -- the output of the parent is connected to the input of the child, and vice versa */
void vk_init_responder(struct vk_thread* parent, struct vk_thread* that, void (*func)(struct vk_thread* that),
                   const char* func_name, char* file, size_t line)
{
	vk_init(that, parent->proc_local_ptr, func, &parent->rx_fd, &parent->tx_fd, func_name, file, line);
	vk_pipeline(parent);
}

int vk_deinit(struct vk_thread* that)
{
	vk_stack_pop(vk_proc_local_get_stack(vk_get_proc_local(that))); /* self */
	vk_stack_pop(vk_proc_local_get_stack(vk_get_proc_local(that))); /* socket */
	return 0;
}

size_t vk_alloc_size() { return sizeof(struct vk_thread); }

vk_func vk_get_func(struct vk_thread* that) { return that->func; }
void vk_set_func(struct vk_thread* that, void (*func)(struct vk_thread* that)) { that->func = func; }
const char* vk_get_func_name(struct vk_thread* that) { return that->func_name; }
void vk_set_func_name(struct vk_thread* that, const char* func_name) { that->func_name = func_name; }
char* vk_get_file(struct vk_thread* that) { return that->file; }
void vk_set_file(struct vk_thread* that, char* file) { that->file = file; }
size_t vk_get_line(struct vk_thread* that) { return that->line; }
void vk_set_line(struct vk_thread* that, size_t line) { that->line = line; }
int vk_get_counter(struct vk_thread* that) { return that->counter; }
void vk_set_counter(struct vk_thread* that, int counter) { that->counter = counter; }
enum VK_PROC_STAT vk_get_status(struct vk_thread* that) { return that->status; }
void vk_set_status(struct vk_thread* that, enum VK_PROC_STAT status) { that->status = status; }
int vk_get_error(struct vk_thread* that) { return that->error; }
void vk_set_error(struct vk_thread* that, int error) { that->error = error; }
void vk_lower_error_ctx(struct vk_thread* that)
{
	vk_set_error(that, 0);
	vk_set_counter(that, vk_get_error_counter(that));
	vk_set_line(   that, vk_get_error_line(that));

}
int vk_get_error_counter(struct vk_thread* that) { return that->error_counter; }
void vk_set_error_counter(struct vk_thread* that, int error_counter) { that->error_counter = error_counter; }
size_t vk_get_error_line(struct vk_thread* that) { return that->error_line; }
void vk_set_error_line(struct vk_thread* that, size_t error_line) { that->error_line = error_line; }
struct vk_proc_local* vk_get_proc_local(struct vk_thread* that) { return that->proc_local_ptr; }
void vk_set_proc_local(struct vk_thread* that, struct vk_proc_local* proc_local_ptr)
{
	that->proc_local_ptr = proc_local_ptr;
}
void* vk_get_self(struct vk_thread* that) { return that->self; }
void vk_set_self(struct vk_thread* that, void* self) { that->self = self; }
struct vk_socket* vk_get_socket(struct vk_thread* that) { return that->socket_ptr; }
void vk_set_socket(struct vk_thread* that, struct vk_socket* socket_ptr) { that->socket_ptr = socket_ptr; }
struct vk_socket* vk_get_waiting_socket(struct vk_thread* that) { return that->waiting_socket_ptr; }
void vk_set_waiting_socket(struct vk_thread* that, struct vk_socket* waiting_socket_ptr)
{
	that->waiting_socket_ptr = waiting_socket_ptr;
}
void vk_ft_enqueue(struct vk_thread* that, struct vk_future* ft_ptr)
{
	TAILQ_INSERT_TAIL(&that->ft_q, ft_ptr, ft_list_elem);
}
struct vk_future* vk_ft_dequeue(struct vk_thread* that)
{
	struct vk_future* ft_ptr;

	ft_ptr = TAILQ_FIRST(&that->ft_q);
	if (ft_ptr != NULL) {
		TAILQ_REMOVE(&that->ft_q, ft_ptr, ft_list_elem);
	}

	return ft_ptr;
}
struct vk_future* vk_ft_peek(struct vk_thread* that)
{
	return TAILQ_FIRST(&that->ft_q);
}
struct vk_pipe* vk_get_rx_fd(struct vk_thread* that) { return &that->rx_fd; }
void vk_set_rx_fd(struct vk_thread* that, struct vk_pipe* rx_fd) { that->rx_fd = *rx_fd; }
struct vk_pipe* vk_get_tx_fd(struct vk_thread* that) { return &that->tx_fd; }
void vk_set_tx_fd(struct vk_thread* that, struct vk_pipe* tx_fd) { that->tx_fd = *tx_fd; }

void vk_enqueue_run(struct vk_thread* that) { vk_proc_local_enqueue_run(that->proc_local_ptr, that); }
int vk_get_enqueued_run(struct vk_thread* that) { return that->run_enq; }
void vk_set_enqueued_run(struct vk_thread* that, int run_enq) { that->run_enq = run_enq; }

struct vk_thread* vk_next_run_vk(struct vk_thread* that) { return SLIST_NEXT(that, run_q_elem); }

int vk_is_completed(struct vk_thread* that) { return that->status == VK_PROC_END; }

int vk_is_ready(struct vk_thread* that) { return that->status == VK_PROC_RUN || that->status == VK_PROC_ERR; }

int vk_is_yielding(struct vk_thread* that) { return that->status == VK_PROC_YIELD; }

/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct vk_thread* that)
{
	vk_dbg("marking thread ready");
	if (that->status == VK_PROC_END) {
		vk_dbg("  but thread has already ended");
	} else if (that->status == VK_PROC_ERR) {
		vk_dbg("  but thread is already catching at error");
	} else {
		that->status = VK_PROC_RUN;
	}
}

ssize_t vk_unblock(struct vk_thread* that)
{
	ssize_t rc;
	vk_dbg("try to unblock thread by applying the blocked I/O operation as far as possible");
	switch (that->status) {
		case VK_PROC_WAIT:
			if (that->waiting_socket_ptr != NULL) {
				rc = vk_socket_handler(that->waiting_socket_ptr);
				if (rc == -1) {
					return -1;
				}
				return rc;
			} else {
				errno = EINVAL;
				return -1;
			}
			break;
		case VK_PROC_LISTEN:
		default:
			break;
	}
	return 0;
}

int vk_copy_arg(struct vk_thread* that, void* src, size_t n)
{
	void *stop;
	size_t capacity;

	stop = vk_stack_get_stop(vk_proc_local_get_stack(vk_get_proc_local(that)));
	capacity = (char *) stop - (char *) that->self;

	if (n > capacity) {
		errno = ENOMEM;
		return -1;
	}

	memcpy(that->self, src, n);

	return 0;
}
