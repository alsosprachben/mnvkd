#include <string.h>

#include "vk_thread.h"
#include "vk_thread_s.h"
#include "vk_future_s.h"

#include "vk_heap.h"
#include "debug.h"

void vk_that_clear(struct vk_thread *that) {
	memset(that, 0, sizeof (*that));
}

void vk_init(struct vk_thread *that, struct vk_proc *proc_ptr, void (*func)(struct vk_thread *that), struct vk_pipe *rx_fd, struct vk_pipe *tx_fd, const char *func_name, char *file, size_t line) {
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
	that->socket_ptr = NULL;
	that->proc_ptr = proc_ptr;
	that->self = vk_heap_get_cursor(vk_proc_get_heap(vk_get_proc(that)));
	that->waiting_socket_ptr = NULL;
	that->ft_q.tqh_first = NULL;
	that->ft_q.tqh_last = NULL;
	TAILQ_INIT(&that->ft_q);
	that->run_q_elem.sle_next = NULL;
	that->run_enq = 0;
}

void vk_init_fds(struct vk_thread *that, struct vk_proc *proc_ptr, void (*func)(struct vk_thread *that), int rx_fd_arg, int tx_fd_arg, const char *func_name, char *file, size_t line) {
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	vk_pipe_init_fd(&rx_fd, rx_fd_arg);
	vk_pipe_init_fd(&tx_fd, tx_fd_arg);
	return vk_init(that, proc_ptr, func, &rx_fd, &tx_fd, func_name, file, line);
}

void vk_init_child(struct vk_thread *parent, struct vk_thread *that, void (*func)(struct vk_thread *that), const char *func_name, char *file, size_t line) {
	return vk_init(that, parent->proc_ptr, func, &parent->rx_fd, &parent->tx_fd, func_name, file, line);
}

int vk_deinit(struct vk_thread *that) {
	return 0;
}

size_t vk_alloc_size() {
	return sizeof (struct vk_thread);
}

vk_func vk_get_func(struct vk_thread *that) {
	return that->func;
}
void vk_set_func(struct vk_thread *that, void (*func)(struct vk_thread *that)) {
	that->func = func;
}
const char *vk_get_func_name(struct vk_thread *that) {
	return that->func_name;
}
void vk_set_func_name(struct vk_thread *that, const char *func_name) {
	that->func_name = func_name;
}
char *vk_get_file(struct vk_thread *that) {
	return that->file;
}
void vk_set_file(struct vk_thread *that, char *file) {
	that->file = file;
}
int vk_get_line(struct vk_thread *that) {
	return that->line;
}
void vk_set_line(struct vk_thread *that, int line) {
	that->line = line;
}
int vk_get_counter(struct vk_thread *that) {
	return that->counter;
}
void vk_set_counter(struct vk_thread *that, int counter) {
	that->counter = counter;
}
enum VK_PROC_STAT vk_get_status(struct vk_thread *that) {
	return that->status;
}
void vk_set_status(struct vk_thread *that, enum VK_PROC_STAT status) {
	that->status = status;
}
int vk_get_error(struct vk_thread *that) {
	return that->error;
}
void vk_set_error(struct vk_thread *that, int error) {
	that->error = error;
}
int vk_get_error_counter(struct vk_thread *that) {
	return that->error_counter;
}
void vk_set_error_counter(struct vk_thread *that, int error_counter) {
	that->error_counter = error_counter;
}
struct vk_proc *vk_get_proc(struct vk_thread *that) {
	return that->proc_ptr;
}
void vk_set_proc(struct vk_thread *that, struct vk_proc *proc_ptr) {
	that->proc_ptr = proc_ptr;
}
void *vk_get_self(struct vk_thread *that) {
	return that->self;
}
void vk_set_self(struct vk_thread *that, void *self) {
	that->self = self;
}
struct vk_socket *vk_get_socket(struct vk_thread *that) {
	return that->socket_ptr;
}
void vk_set_socket(struct vk_thread *that, struct vk_socket *socket_ptr) {
	that->socket_ptr = socket_ptr;
}
struct vk_socket *vk_get_waiting_socket(struct vk_thread *that) {
	return that->waiting_socket_ptr;
}
void vk_set_waiting_socket(struct vk_thread *that, struct vk_socket *waiting_socket_ptr) {
	that->waiting_socket_ptr = waiting_socket_ptr;
}
void vk_ft_enqueue(struct vk_thread *that, struct vk_future *ft_ptr) {
	TAILQ_INSERT_TAIL(&that->ft_q, ft_ptr, ft_list_elem);
}
struct vk_future *vk_ft_dequeue(struct vk_thread *that) {
	struct vk_future *ft_ptr;

	ft_ptr = TAILQ_FIRST(&that->ft_q);
	if (ft_ptr != NULL) {
		TAILQ_REMOVE(&that->ft_q, ft_ptr, ft_list_elem);
	}

	return ft_ptr;
}
struct vk_pipe* vk_get_rx_fd(struct vk_thread *that) {
	return &that->rx_fd;
}
void vk_set_rx_fd(struct vk_thread *that, struct vk_pipe *rx_fd) {
	that->rx_fd = *rx_fd;
}
struct vk_pipe *vk_get_tx_fd(struct vk_thread *that) {
	return &that->tx_fd;
}
void vk_set_tx_fd(struct vk_thread *that, struct vk_pipe *tx_fd) {
	that->tx_fd = *tx_fd;
}

void vk_enqueue_run(struct vk_thread *that) {
	vk_proc_enqueue_run(that->proc_ptr, that);
}
int vk_get_enqueued_run(struct vk_thread *that) {
	return that->run_enq;
}
void vk_set_enqueued_run(struct vk_thread *that, int run_enq) {
	that->run_enq = run_enq;
}

struct vk_thread *vk_next_run_vk(struct vk_thread *that) {
	return SLIST_NEXT(that, run_q_elem);
}

int vk_is_completed(struct vk_thread *that) {
	return that->status == VK_PROC_END;
}

int vk_is_ready(struct vk_thread *that) {
	return that->status == VK_PROC_RUN || that->status == VK_PROC_ERR;
}

int vk_is_yielding(struct vk_thread *that) {
	return that->status == VK_PROC_YIELD;
}

/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct vk_thread *that) {
	DBG(" READY@"PRIvk"\n", ARGvk(that));
	if (that->status == VK_PROC_END) {
		DBG(" ENDED@"PRIvk"\n", ARGvk(that));
	} else if (that->status == VK_PROC_ERR) {
		DBG(" ERRED@"PRIvk"\n", ARGvk(that));
	} else {
		that->status = VK_PROC_RUN;
	}
}

ssize_t vk_unblock(struct vk_thread *that) {
	ssize_t rc;
	switch (that->status) {
		case VK_PROC_WAIT:
			if (that->waiting_socket_ptr != NULL) {
				/*
				vk_vectoring_printf(&that->waiting_socket_ptr->rx.ring, "pre-rx");
				vk_vectoring_printf(&that->waiting_socket_ptr->tx.ring, "pre-tx");
				*/

				rc = vk_socket_handler(that->waiting_socket_ptr);
				if (rc == -1) {
					return -1;
				}

				/*
				vk_vectoring_printf(&that->waiting_socket_ptr->rx.ring, "post-rx");
				vk_vectoring_printf(&that->waiting_socket_ptr->tx.ring, "post-tx");
				*/
				return rc;
			} else {
				errno = EINVAL;
				return -1;
			}
			break;
		case VK_PROC_LISTEN:
			break;
		default:
			break;
	}
	return 0;
}

void vk_deblock_waiting_socket(struct vk_thread *that) {
	if (vk_get_waiting_socket(that) != NULL && vk_socket_get_enqueued_blocked(vk_get_waiting_socket(that))) {
		vk_proc_drop_blocked(vk_get_proc(that), vk_get_waiting_socket(that));
	}
}

void vk_deblock_socket(struct vk_thread *that) {
	if (vk_get_socket(that) != NULL && vk_socket_get_enqueued_blocked(vk_get_socket(that))) {
		vk_proc_drop_blocked(vk_get_proc(that), vk_get_socket(that));
	}
}

void vk_derun(struct vk_thread *that) {
	if (vk_get_enqueued_run(that)) {
		vk_proc_drop_run(vk_get_proc(that), that);
	}
}

int vk_copy_arg(struct vk_thread *that, void *src, size_t n) {
	size_t capacity;

	capacity = vk_heap_get_free(vk_proc_get_heap(vk_get_proc(that)));

	if (n > capacity) {
		errno = ENOMEM;
		return -1;
	}

	memcpy(that->self, src, n);

	return 0;
}