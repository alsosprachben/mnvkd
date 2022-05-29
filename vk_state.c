#include "vk_state.h"
#include "vk_state_s.h"

#include "vk_heap.h"
#include "debug.h"

void vk_init(struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), struct vk_pipe *rx_fd, struct vk_pipe *tx_fd, const char *func_name, char *file, size_t line) {
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
	that->ft_ptr = NULL;
	that->run_q_elem.sle_next = NULL;
	that->run_enq = 0;
}

void vk_init_fds(struct that *that, struct vk_proc *proc_ptr, void (*func)(struct that *that), int rx_fd_arg, int tx_fd_arg, const char *func_name, char *file, size_t line) {
	struct vk_pipe rx_fd;
	struct vk_pipe tx_fd;
	vk_pipe_init_fd(&rx_fd, rx_fd_arg);
	vk_pipe_init_fd(&tx_fd, tx_fd_arg);
	return vk_init(that, proc_ptr, func, &rx_fd, &tx_fd, func_name, file, line);
}

void vk_init_child(struct that *parent, struct that *that, void (*func)(struct that *that), const char *func_name, char *file, size_t line) {
	return vk_init(that, parent->proc_ptr, func, &parent->rx_fd, &parent->tx_fd, func_name, file, line);
}

int vk_deinit(struct that *that) {
	return 0;
}

size_t vk_alloc_size() {
	return sizeof (struct that);
}

vk_func vk_get_func(struct that *that) {
	return that->func;
}
void vk_set_func(struct that *that, void (*func)(struct that *that)) {
	that->func = func;
}
const char *vk_get_func_name(struct that *that) {
	return that->func_name;
}
void vk_set_func_name(struct that *that, const char *func_name) {
	that->func_name = func_name;
}
char *vk_get_file(struct that *that) {
	return that->file;
}
void vk_set_file(struct that *that, char *file) {
	that->file = file;
}
int vk_get_line(struct that *that) {
	return that->line;
}
void vk_set_line(struct that *that, int line) {
	that->line = line;
}
int vk_get_counter(struct that *that) {
	return that->counter;
}
void vk_set_counter(struct that *that, int counter) {
	that->counter = counter;
}
enum VK_PROC_STAT vk_get_status(struct that *that) {
	return that->status;
}
void vk_set_status(struct that *that, enum VK_PROC_STAT status) {
	that->status = status;
}
int vk_get_error(struct that *that) {
	return that->error;
}
void vk_set_error(struct that *that, int error) {
	that->error = error;
}
int vk_get_error_counter(struct that *that) {
	return that->error_counter;
}
void vk_set_error_counter(struct that *that, int error_counter) {
	that->error_counter = error_counter;
}
struct vk_proc *vk_get_proc(struct that *that) {
	return that->proc_ptr;
}
void vk_set_proc(struct that *that, struct vk_proc *proc_ptr) {
	that->proc_ptr = proc_ptr;
}
void *vk_get_self(struct that *that) {
	return that->self;
}
void vk_set_self(struct that *that, void *self) {
	that->self = self;
}
struct vk_socket *vk_get_socket(struct that *that) {
	return that->socket_ptr;
}
void vk_set_socket(struct that *that, struct vk_socket *socket_ptr) {
	that->socket_ptr = socket_ptr;
}
struct vk_socket *vk_get_waiting_socket(struct that *that) {
	return that->waiting_socket_ptr;
}
void vk_set_waiting_socket(struct that *that, struct vk_socket *waiting_socket_ptr) {
	that->waiting_socket_ptr = waiting_socket_ptr;
}
struct future *vk_get_future(struct that *that) {
	return that->ft_ptr;
}
void vk_set_future(struct that *that, struct future *ft_ptr) {
	that->ft_ptr = ft_ptr;
}
struct vk_pipe* vk_get_rx_fd(struct that *that) {
	return &that->rx_fd;
}
void vk_set_rx_fd(struct that *that, struct vk_pipe *rx_fd) {
	that->rx_fd = *rx_fd;
}
struct vk_pipe *vk_get_tx_fd(struct that *that) {
	return &that->tx_fd;
}
void vk_set_tx_fd(struct that *that, struct vk_pipe *tx_fd) {
	that->tx_fd = *tx_fd;
}

void vk_enqueue_run(struct that *that) {
	vk_proc_enqueue_run(that->proc_ptr, that);
}
int vk_get_enqueued_run(struct that *that) {
	return that->run_enq;
}
void vk_set_enqueued_run(struct that *that, int run_enq) {
	that->run_enq = run_enq;
}

int vk_is_completed(struct that *that) {
	return that->status == VK_PROC_END;
}

int vk_is_ready(struct that *that) {
	return that->status == VK_PROC_RUN || that->status == VK_PROC_ERR;
}

int vk_is_yielding(struct that *that) {
	return that->status == VK_PROC_YIELD;
}

/* set coroutine status to VK_PROC_RUN */
void vk_ready(struct that *that) {
	DBG(" READY@"PRIvk"\n", ARGvk(that));
	if (that->status == VK_PROC_END) {
		DBG(" ENDED@"PRIvk"\n", ARGvk(that));
	} else if (that->status == VK_PROC_ERR) {
		DBG(" ERRED@"PRIvk"\n", ARGvk(that));
	} else {
		that->status = VK_PROC_RUN;
	}
}

ssize_t vk_unblock(struct that *that) {
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
		default:
			break;
	}
	return 0;
}

