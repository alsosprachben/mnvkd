#include "vk_thread.h"
#include "vk_thread_io.h"
#include "vk_socket.h"
#include "vk_io_queue.h"
#include "vk_io_queue_s.h"
#include "vk_io_op.h"
#include "vk_io_op_s.h"
#include "vk_io_exec.h"
#include <stdlib.h>
#include <errno.h>

static int
vk_main_local_process_deferred(struct vk_proc_local* proc_local_ptr)
{
	int progressed = 0;
	struct vk_thread* thread_ptr = vk_proc_local_first_deferred(proc_local_ptr);

	while (thread_ptr) {
		struct vk_thread* next_thread = vk_next_deferred_vk(thread_ptr);
		struct vk_socket* socket_ptr = vk_get_waiting_socket(thread_ptr);
		struct vk_io_queue* queue_ptr = socket_ptr ? vk_socket_get_io_queue(socket_ptr) : NULL;

		if (!queue_ptr) {
			thread_ptr = next_thread;
			continue;
		}

		struct vk_io_op* op_ptr;
		while ((op_ptr = vk_io_queue_pop_virt(queue_ptr))) {
			ssize_t virt_rc = vk_socket_handler(socket_ptr);
			if (virt_rc == -1) {
				op_ptr->res = -1;
				op_ptr->err = errno;
				op_ptr->state = VK_IO_OP_ERROR;
			} else {
				op_ptr->res = 0;
				op_ptr->err = 0;
				op_ptr->state = VK_IO_OP_DONE;
			}
			vk_thread_io_complete_op(socket_ptr, queue_ptr, op_ptr);
			progressed = 1;
		}

		while ((op_ptr = vk_io_queue_pop_phys(queue_ptr))) {
			if (vk_io_exec_rw(op_ptr) == -1 && op_ptr->state == VK_IO_OP_PENDING) {
				op_ptr->res = -1;
				op_ptr->err = errno;
				op_ptr->state = VK_IO_OP_ERROR;
			}
			vk_thread_io_complete_op(socket_ptr, queue_ptr, op_ptr);
			progressed = 1;
		}

		thread_ptr = next_thread;
	}

	return progressed;
}

int vk_main_local_init(vk_func main_vk, void *arg_buf, size_t arg_len, size_t page_count)
{
	int rc = 0;
	struct vk_heap *heap_ptr;
	struct vk_proc_local* proc_local_ptr;
	struct vk_thread *that;

	do { /* for cleanup break */
		heap_ptr = calloc(1, vk_heap_alloc_size());
		if (heap_ptr == NULL) {
			perror("calloc");
			rc = -1;
			break;
		}

		rc = vk_heap_map(heap_ptr, NULL, page_count * 4096, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0, 1);
		if (rc == -1) {
			perror("vk_heap_map");
			break;
		}

		proc_local_ptr = vk_stack_push(vk_heap_get_stack(heap_ptr), 1, vk_proc_local_alloc_size());
		if (proc_local_ptr == NULL) {
			perror("vk_stack_push");
			rc = -1;
			break;
		}

		vk_proc_local_init(proc_local_ptr);
		vk_proc_local_set_proc_id(proc_local_ptr, 1);
		vk_proc_local_set_stack(proc_local_ptr, vk_heap_get_stack(heap_ptr));

		that = vk_stack_push(vk_proc_local_get_stack(proc_local_ptr), 1, vk_alloc_size());
		if (that == NULL) {
			perror("vk_stack_push");
			rc = -1;
			break;
		}

		/*
		 * Initialize the main coroutine with stdin and stdout bound to the default socket.
		 * Leave the FDs in blocking mode, since we have no poller.
		 */
		VK_INIT(that, proc_local_ptr, main_vk, 0, VK_FD_TYPE_SOCKET_STREAM, 1,
		        VK_FD_TYPE_SOCKET_STREAM);
		rc = vk_copy_arg(that, arg_buf, arg_len);
		if (rc == -1) {
			perror("vk_copy_arg");
			break;
		}

	/* enqueue it to run */
	vk_proc_local_enqueue_run(proc_local_ptr, that);

	while (!vk_proc_local_is_zombie(proc_local_ptr)) {
		rc = vk_proc_local_execute(proc_local_ptr);
		if (rc == -1) {
			perror("vk_proc_local_execute");
			break;
		}

		if (vk_proc_local_is_zombie(proc_local_ptr)) {
			break;
		}

		if (!vk_main_local_process_deferred(proc_local_ptr) &&
		    vk_proc_local_first_deferred(proc_local_ptr) != NULL) {
			errno = EWOULDBLOCK;
			perror("vk_main_local_process_deferred");
			rc = -1;
			break;
		}
	}
	} while (0);
	if (heap_ptr != NULL) {
		free(heap_ptr);
	}

	return rc;
}
