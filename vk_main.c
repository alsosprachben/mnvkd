#include <fcntl.h>
#include <stdlib.h>

#include "vk_kern.h"
#include "vk_heap.h"
#include "vk_fd_table.h"
#include "vk_proc.h"
#include "vk_server_s.h"
#include "vk_socket.h"

#include "vk_main.h"

int vk_main_init(vk_func main_vk, void *arg_buf, size_t arg_len, size_t page_count, int privileged, int isolated)
{
	int rc = 0;
	struct vk_heap* kern_heap_ptr;
	struct vk_kern* kern_ptr;
	struct vk_proc* proc_ptr;
	struct vk_thread* vk_ptr;
	char* vk_poll_driver;
	char* vk_poll_method;

	do { /* for cleanup break */
		kern_heap_ptr = calloc(1, vk_heap_alloc_size());
		if (kern_heap_ptr == NULL) {
			ERR("Failed to allocate memory for kern_heap_ptr.\n");
			return -1;
		}
		kern_ptr = vk_kern_alloc(kern_heap_ptr);
		if (kern_ptr == NULL) {
			rc = -1;
			break;
		}

		proc_ptr = vk_kern_alloc_proc(kern_ptr, NULL);
		if (proc_ptr == NULL) {
			rc = -1;
			break;
		}
		rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * page_count, 0, 1);
		if (rc == -1) {
			break;
		}
		vk_proc_set_privileged(proc_ptr,
		                       privileged); /* needs access to kernel to spawn processes for accepted connections */
		vk_proc_set_isolated(proc_ptr, isolated);

		vk_ptr = vk_proc_alloc_thread(proc_ptr);
		if (vk_ptr == NULL) {
			rc = -1;
			break;
		}

		/*
		 * Initialize the main coroutine with stdin and stdout bound to the default socket.
		 */
		fcntl(0, F_SETFL, O_NONBLOCK);
		fcntl(1, F_SETFL, O_NONBLOCK);

		VK_INIT(vk_ptr, vk_proc_get_local(proc_ptr), main_vk, 0, VK_FD_TYPE_SOCKET_STREAM, 1,
		        VK_FD_TYPE_SOCKET_STREAM);
		rc = vk_copy_arg(vk_ptr, arg_buf, arg_len);
		if (rc == -1) {
			break;
		}

		/* enqueue it to run */
		vk_proc_local_enqueue_run(vk_proc_get_local(proc_ptr), vk_ptr);
		/* flush run status to kernel */
		vk_kern_flush_proc_queues(kern_ptr, proc_ptr);

		vk_poll_driver = getenv("VK_POLL_DRIVER");
		if (vk_poll_driver != NULL && strcmp(vk_poll_driver, "OS") == 0) {
			vk_fd_table_set_poll_driver(vk_kern_get_fd_table(kern_ptr), VK_POLL_DRIVER_OS);
			ERR("Using OS-specific poller.\n");
		}
		vk_poll_method = getenv("VK_POLL_METHOD");
		if (vk_poll_method != NULL && strcmp(vk_poll_method, "EDGE_TRIGGERED") == 0) {
			vk_fd_table_set_poll_method(vk_kern_get_fd_table(kern_ptr), VK_POLL_METHOD_EDGE_TRIGGERED);
			ERR("Using edge-triggered polling.\n");
		}

		rc = vk_kern_loop(kern_ptr);
		if (rc == -1) {
			break;
		}
		
		rc = vk_proc_free(proc_ptr);
		if (rc == -1) {
			break;
		}

		vk_kern_free_proc(kern_ptr, proc_ptr);
	} while (0);
	if (kern_heap_ptr != NULL) {
		free(kern_heap_ptr);
	}

	return rc;
}

