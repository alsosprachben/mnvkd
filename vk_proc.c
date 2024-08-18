/* Copyright 2022 BCW. All Rights Reserved. */
#include <string.h>

#include "vk_debug.h"

#include "vk_proc.h"
#include "vk_proc_local.h"
#include "vk_proc_local_s.h"
#include "vk_proc_s.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "vk_thread.h"
#include "vk_thread_s.h"

#include "vk_fd.h"
#include "vk_fd_table.h"
#include "vk_heap.h"
#include "vk_heap_s.h"
#include "vk_kern.h"
#include "vk_pool.h"
#include "vk_wrapguard.h"

/*
 * Object Manipulation
 */

void vk_proc_clear(struct vk_proc* proc_ptr)
{
	size_t proc_id;

	proc_id = proc_ptr->proc_id;
	memset(proc_ptr, 0, sizeof(*proc_ptr));
	proc_ptr->proc_id = proc_id;
}

struct vk_proc_local* vk_proc_get_local(struct vk_proc* proc_ptr) { return proc_ptr->local_ptr; }

void vk_proc_init(struct vk_proc* proc_ptr)
{
	proc_ptr->run_qed = 0;
	proc_ptr->blocked_qed = 0;

	vk_proc_local_init(proc_ptr->local_ptr);
}

size_t vk_proc_get_id(struct vk_proc* proc_ptr) { return proc_ptr->proc_id; }
void vk_proc_set_id(struct vk_proc* proc_ptr, size_t proc_id) { proc_ptr->proc_id = proc_id; }

size_t vk_proc_get_pool_entry_id(struct vk_proc* proc_ptr) { return proc_ptr->pool_entry_id; }
void vk_proc_set_pool_entry_id(struct vk_proc* proc_ptr, size_t pool_entry_id)
{
	proc_ptr->pool_entry_id = pool_entry_id;
}
struct vk_pool* vk_proc_get_pool(struct vk_proc* proc_ptr) { return proc_ptr->pool_ptr; }
void vk_proc_set_pool(struct vk_proc* proc_ptr, struct vk_pool* pool_ptr) { proc_ptr->pool_ptr = pool_ptr; }
struct vk_pool_entry* vk_proc_get_entry(struct vk_proc* proc_ptr) { return proc_ptr->entry_ptr; }
void vk_proc_set_entry(struct vk_proc* proc_ptr, struct vk_pool_entry* entry_ptr) { proc_ptr->entry_ptr = entry_ptr; }

int vk_proc_get_run(struct vk_proc* proc_ptr) { return proc_ptr->run_qed; }
void vk_proc_set_run(struct vk_proc* proc_ptr, int run) { proc_ptr->run_qed = run; }

int vk_proc_get_blocked(struct vk_proc* proc_ptr) { return proc_ptr->blocked_qed; }
void vk_proc_set_blocked(struct vk_proc* proc_ptr, int blocked) { proc_ptr->blocked_qed = blocked; }

int vk_proc_is_zombie(struct vk_proc* proc_ptr) { return !(proc_ptr->run_qed || proc_ptr->blocked_qed); }

struct vk_fd* vk_proc_first_fd(struct vk_proc* proc_ptr) { return SLIST_FIRST(&proc_ptr->allocated_fds); }

int vk_proc_allocate_fd(struct vk_proc* proc_ptr, struct vk_fd* fd_ptr, int fd)
{
	vk_proc_dbgf("allocating FD %i to process\n", fd);
	if (!vk_fd_get_allocated(fd_ptr)) {
		vk_fd_allocate(fd_ptr, fd, proc_ptr->proc_id);
		SLIST_INSERT_HEAD(&proc_ptr->allocated_fds, fd_ptr, allocated_list_elem);
		vk_fd_dbg("allocated");
	} else {
		if (vk_fd_get_proc_id(fd_ptr) != proc_ptr->proc_id) {
			errno = EEXIST;
			vk_fd_dbg("already allocated to another process");
			return -1;
		}
	}
	return 0;
}

void vk_proc_deallocate_fd(struct vk_proc* proc_ptr, struct vk_fd* fd_ptr)
{
	vk_fd_dbgf("deallocating from process %zu\n", proc_ptr->proc_id);
	if (vk_fd_get_allocated(fd_ptr)) {
		vk_fd_set_allocated(fd_ptr, 0);
		SLIST_REMOVE(&proc_ptr->allocated_fds, fd_ptr, vk_fd, allocated_list_elem);
		vk_proc_dbg("deallocated");
	}
}

int vk_proc_get_privileged(struct vk_proc* proc_ptr) { return proc_ptr->privileged; }
void vk_proc_set_privileged(struct vk_proc* proc_ptr, int privileged) { proc_ptr->privileged = privileged; }

int vk_proc_get_isolated(struct vk_proc* proc_ptr) { return proc_ptr->isolated; }
void vk_proc_set_isolated(struct vk_proc* proc_ptr, int isolated) { proc_ptr->isolated = isolated; }

int vk_proc_alloc(struct vk_proc* proc_ptr, void* map_addr, size_t map_len, int map_prot, int map_flags, int map_fd,
		  off_t map_offset, int entered, int collapse)
{
	int rc;
	size_t alignedlen;
	size_t hugealignedlen;
	struct vk_heap heap;
	memset(&heap, 0, sizeof(heap));

	vk_proc_dbg("allocating");

	rc = vk_safe_alignedlen(1, map_len, &alignedlen);
	if (rc == -1) {
		return -1;
	}

#ifdef MADV_HUGEPAGE
	if (collapse) {
		rc = vk_safe_hugealignedlen(1, alignedlen, &hugealignedlen);
		if (rc == -1) {
			return -1;
		}
		vk_proc_dbgf("alignedlen: %zu, hugealignedlen: %zu\n", alignedlen, hugealignedlen);
	} else {
		hugealignedlen = alignedlen;
	}
#else
	hugealignedlen = alignedlen;
#endif

	rc = vk_heap_map(&heap, map_addr, hugealignedlen, map_prot, map_flags, map_fd, map_offset, entered);
	if (rc == -1) {
		return -1;
	}

#ifdef MADV_HUGEPAGE
	if (collapse) {
#ifdef MADV_COLLAPSE
		rc = vk_heap_advise(&heap, MADV_COLLAPSE);
		if (rc == -1) {
			vk_proc_perror("vk_heap_advise");
		}
#else
		vk_proc_log("MADV_COLLAPSE not defined, skipping...");
#endif
		rc = vk_heap_advise(&heap, MADV_HUGEPAGE);
		if (rc == -1) {
			vk_proc_perror("vk_heap_advise");
		}
	}
#endif

	if (!entered) {
		rc = vk_heap_enter(&heap);
		if (rc == -1) {
			return -1;
		}
	}
	proc_ptr->local_ptr = vk_stack_push(vk_heap_get_stack(&heap), 1, vk_proc_local_alloc_size());
	if (proc_ptr->local_ptr == NULL) {
		int errno2;
		errno2 = errno;
		vk_heap_unmap(&heap);
		errno = errno2;
		return -1;
	}

	vk_proc_local_set_proc_id(vk_proc_get_local(proc_ptr), proc_ptr->proc_id);
	vk_proc_local_set_stack(vk_proc_get_local(proc_ptr), &heap.stack);

	vk_proc_init(proc_ptr);

	if (!entered) {
		rc = vk_heap_exit(&heap);
		if (rc == -1) {
			return -1;
		}
	}

	proc_ptr->heap = heap;

	vk_proc_dbg("allocated");

	return 0;
}

int vk_proc_alloc_from_pool(struct vk_proc* proc_ptr, struct vk_pool* pool_ptr)
{
	struct vk_pool_entry* entry_ptr;

	vk_proc_dbg("allocating from pool");

	entry_ptr = vk_pool_alloc_entry(pool_ptr);
	if (entry_ptr == NULL) {
		return -1;
	}

	proc_ptr->heap = *vk_pool_entry_get_heap(entry_ptr);
	vk_heap_enter(vk_proc_get_heap(proc_ptr));

	proc_ptr->local_ptr =
	    vk_stack_push(vk_heap_get_stack(vk_proc_get_heap(proc_ptr)), 1, vk_proc_local_alloc_size());
	if (proc_ptr->local_ptr == NULL) {
		int errno2;
		errno2 = errno;
		vk_pool_free_entry(pool_ptr, entry_ptr);
		errno = errno2;
		return -1;
	}

	memset(proc_ptr->local_ptr, 0, vk_proc_local_alloc_size());

	vk_proc_local_set_proc_id(vk_proc_get_local(proc_ptr), proc_ptr->proc_id);
	vk_proc_local_set_stack(vk_proc_get_local(proc_ptr), vk_heap_get_stack(vk_proc_get_heap(proc_ptr)));

	proc_ptr->pool_entry_id = vk_pool_entry_get_id(entry_ptr);
	proc_ptr->pool_ptr = pool_ptr;
	proc_ptr->entry_ptr = entry_ptr;

	vk_proc_init(proc_ptr);

	vk_proc_dbg("allocated from pool");

	return 0;
}

int vk_proc_free(struct vk_proc* proc_ptr)
{
	int rc;

	rc = vk_heap_unmap(vk_proc_get_heap(proc_ptr));
	if (rc == -1) {
		return -1;
	}

	return rc;
}

int vk_proc_free_from_pool(struct vk_proc* proc_ptr, struct vk_pool* pool_ptr)
{
	struct vk_pool_entry* entry_ptr;

	entry_ptr = vk_pool_get_entry(pool_ptr, proc_ptr->pool_entry_id);
	if (entry_ptr == NULL) {
		return -1;
	}

	return vk_pool_free_entry(pool_ptr, entry_ptr);
}

struct vk_thread* vk_proc_alloc_thread(struct vk_proc* proc_ptr)
{
	int rc;
	struct vk_thread* that;

	rc = vk_heap_enter(vk_proc_get_heap(proc_ptr));
	if (rc == -1) {
		return NULL;
	}

	that = vk_stack_push(vk_proc_local_get_stack(proc_ptr->local_ptr), 1, sizeof(*that));
	if (that == NULL) {
		return NULL;
	}

	/* leave heap open */

	return that;
}

int vk_proc_free_thread(struct vk_proc* proc_ptr) { return vk_stack_pop(vk_proc_local_get_stack(proc_ptr->local_ptr)); }

size_t vk_proc_alloc_size() { return sizeof(struct vk_proc); }

struct vk_heap* vk_proc_get_heap(struct vk_proc* proc_ptr) { return &proc_ptr->heap; }

struct vk_proc* vk_proc_next_run_proc(struct vk_proc* proc_ptr) { return SLIST_NEXT(proc_ptr, run_list_elem); }

struct vk_proc* vk_proc_next_blocked_proc(struct vk_proc* proc_ptr) { return SLIST_NEXT(proc_ptr, blocked_list_elem); }

void vk_proc_dump_fd_q(struct vk_proc* proc_ptr)
{
	struct vk_fd* fd_ptr;
	struct vk_socket* socket_ptr;
	SLIST_FOREACH(fd_ptr, &proc_ptr->allocated_fds, allocated_list_elem)
	{
		vk_fd_log("allocated");
		socket_ptr = vk_io_future_get_socket(vk_fd_get_ioft_pre(fd_ptr));
		if (socket_ptr != NULL) {
			vk_socket_log("for process FD");
		}
	}
}

/*
 * I/O polling glue, and process execution
 */

/*
 * Polling hook for after process execution, before the event loop.
 * Polling registration state is in the process, and needs to be registered with the poller.
 *
 * Iterate over each of the process's allocated FDs,
 *   to handle closure status.
 * Iterate over each of the process's blocked sockets,
 *   to register each socket with the poller.
 */
int vk_proc_prepoll(struct vk_proc* proc_ptr, struct vk_fd_table* fd_table_ptr)
{
	struct vk_socket* socket_ptr;
	struct vk_fd* cursor_fd_ptr;
	struct vk_fd* fd_ptr;
	struct vk_proc_local* proc_local_ptr;

	vk_proc_dbg("prepoll");

	/* Deregister and deallocate any closed FDs associated with this process. */
	cursor_fd_ptr = vk_proc_first_fd(proc_ptr);
	while (cursor_fd_ptr) {
		fd_ptr = cursor_fd_ptr;
		/*
		 * Deregister closed FD, if it is closed.
		 * just enqueue it, and poller will react to closed status
		 */
		if (vk_fd_get_closed(fd_ptr)) {
			vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
		}

		/* Walk the link prior to deallocation
		 * because the link is destroyed by deallocation.
		 */
		cursor_fd_ptr = vk_fd_next_allocated_fd(cursor_fd_ptr);
		if (vk_fd_get_closed(fd_ptr)) {
			vk_proc_deallocate_fd(proc_ptr, fd_ptr);
		}
	}

	proc_local_ptr = vk_proc_get_local(proc_ptr);

	/* Register each of the process's blocked sockets with the poller. */
	socket_ptr = vk_proc_local_first_blocked(proc_local_ptr);
	while (socket_ptr) {
		vk_fd_table_prepoll_blocked_socket(fd_table_ptr, socket_ptr, proc_ptr);

		socket_ptr = vk_socket_next_blocked_socket(socket_ptr);
	}

	return 0;
}

/*
 * Polling hook for after the event loop, before process execution.
 * Polling results are in the kernel's FD table, and need to be applied to the process's blocked sockets.
 *
 * Iterate over each of the process's blocked sockets,
 *   to apply poll results to each socket, and
 *   to enqueue each socket's blocked thread for retry.
 */
int vk_proc_postpoll(struct vk_proc* proc_ptr, struct vk_fd_table* fd_table_ptr)
{
	struct vk_socket* socket_ptr;
	struct vk_proc_local* proc_local_ptr;
	struct vk_io_future* rx_ioft_pre_ptr;
	struct vk_io_future* tx_ioft_pre_ptr;
	struct vk_io_future* rx_ioft_ret_ptr;
	struct vk_io_future* tx_ioft_ret_ptr;
	struct vk_fd* fd_ptr;
	int rc;
	int rx_fd;
	int tx_fd;

	vk_proc_dbg("prepoll");

	proc_local_ptr = vk_proc_get_local(proc_ptr);

	socket_ptr = vk_proc_local_first_blocked(proc_local_ptr);
	while (socket_ptr) {
		rx_ioft_pre_ptr = vk_block_get_ioft_rx_pre(vk_socket_get_block(socket_ptr));
		tx_ioft_pre_ptr = vk_block_get_ioft_tx_pre(vk_socket_get_block(socket_ptr));
		rx_ioft_ret_ptr = vk_block_get_ioft_rx_ret(vk_socket_get_block(socket_ptr));
		tx_ioft_ret_ptr = vk_block_get_ioft_tx_ret(vk_socket_get_block(socket_ptr));

		rx_fd = vk_io_future_get_event(rx_ioft_pre_ptr).fd;
		tx_fd = vk_io_future_get_event(tx_ioft_pre_ptr).fd;

		if (rx_fd != -1) {
			vk_socket_dbgf("Socket has a read FD %i.\n", rx_fd);

			fd_ptr = vk_fd_table_get(fd_table_ptr, rx_fd);
			if (fd_ptr == NULL) {
				vk_socket_logf("PANIC!!! FD %i is not registered with the FD table.\n", rx_fd);
				return -1;
			}

			vk_fd_dbg("Applying poll results to socket's blocked future.");

			vk_io_future_set_event(rx_ioft_ret_ptr, vk_io_future_get_event(vk_fd_get_ioft_ret(fd_ptr)));
		}

		if (tx_fd != -1) {
			vk_socket_dbgf("Socket has a write FD %i.\n", tx_fd);

			fd_ptr = vk_fd_table_get(fd_table_ptr, tx_fd);
			if (fd_ptr == NULL) {
				vk_socket_logf("PANIC!!! FD %i is not registered with the FD table.\n", tx_fd);
				return -1;
			}

			vk_fd_dbg("Applying poll results to socket's blocked future.");

			vk_io_future_set_event(tx_ioft_ret_ptr, vk_io_future_get_event(vk_fd_get_ioft_ret(fd_ptr)));
		}

		socket_ptr = vk_socket_next_blocked_socket(socket_ptr);
	}

	socket_ptr = vk_proc_local_first_blocked(proc_local_ptr);
	while (socket_ptr) {
		rc = vk_proc_local_retry_socket(proc_local_ptr, socket_ptr);
		if (rc == -1) {
			return -1;
		}

		socket_ptr = vk_socket_next_blocked_socket(socket_ptr);
	}

	return 0;
}

/*
 * Process Execution, with
 *  - per-process polling hooks,
 *  - kernel heap protection, and
 *  - signal raising, around
 *  - process-local execution.
 */
int vk_proc_execute(struct vk_proc* proc_ptr, struct vk_kern* kern_ptr)
{
	int rc;
	struct vk_proc_local* proc_local_ptr;
	struct vk_fd_table* fd_table_ptr;
	int privileged;
	proc_local_ptr = vk_proc_get_local(proc_ptr);
	fd_table_ptr = vk_kern_get_fd_table(kern_ptr);

	if (proc_local_ptr == NULL) {
		vk_proc_log("proc_local_ptr == NULL");
		return 0;
	}
	/* If signal is raised inside the process, we go directly to re-execution. */
	rc = vk_proc_local_raise_signal(proc_local_ptr);
	if (!rc) {
		rc = vk_proc_postpoll(proc_ptr, fd_table_ptr);
		if (rc == -1) {
			vk_proc_perror("vk_proc_postpoll");
			return -1;
		}
	}

	privileged = proc_ptr->privileged; /* read while kernel is accessible */
	if (!privileged) {
		vk_proc_dbg("exiting kernel heap to enter protected mode");
		/* open visible kern heap */
		vk_kern_mainline_udata_open_kern_hd((vk_signal_get_mainline_udata()));
		/* exit visible kern heap */
		vk_heap_exit(vk_kern_mainline_udata_get_kern_hd(vk_signal_get_mainline_udata()));
	} else {
		vk_proc_dbg("skipping kernel heap exit, because process is privileged");
	}

	rc = vk_proc_local_execute(proc_local_ptr);

	if (!privileged) {
		/* enter visible kern heap */
		vk_heap_enter(vk_kern_mainline_udata_get_kern_hd(vk_signal_get_mainline_udata()));
		vk_proc_dbg("entered kernel heap to exit protected mode");
	} else {
		vk_proc_dbg("skipping kernel heap enter, because process is privileged");
	}

	if (rc == -1) {
		vk_proc_perror("vk_proc_local_execute");
		return -1;
	}

	rc = vk_proc_prepoll(proc_ptr, fd_table_ptr);
	if (rc == -1) {
		vk_proc_perror("vk_proc_prepoll");
		return -1;
	}

	return 0;
}