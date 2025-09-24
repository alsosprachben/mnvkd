/* Copyright 2022 BCW. All Rights Reserved. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <poll.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <errno.h>

#include "vk_kern.h"
#include "vk_kern_s.h"

#include "vk_heap.h"
#include "vk_heap_s.h"

#include "vk_proc_local.h"
#include "vk_socket.h"
#include "vk_thread.h"
#include "vk_thread_err.h"

#include "vk_signal.h"
#include "vk_wrapguard.h"
#include "vk_huge.h"
#include "vk_huge_s.h"
#include "vk_isolate.h"

/* isolate scheduler callback: re-enqueue the current running thread of this proc */
static void vk_isolate_scheduler_cb(void *ud)
{
    struct vk_proc_local* pl = (struct vk_proc_local*)ud;
    if (!pl) return;
    struct vk_thread* that = vk_proc_local_get_running(pl);
    if (!that) return;

    /*
     * Only re-enqueue truly yielding coroutines. For WAIT (I/O wait),
     * leave status unchanged so the engine can run vk_unblock() and
     * register/poll the FD properly. Flipping WAIT->RUN here causes
     * the coroutine to spin without ever registering I/O.
     */
    if (vk_is_yielding(that)) {
        vk_proc_local_enqueue_run(pl, that);
    }
}

void vk_kern_mainline_udata_set_kern(struct vk_kern_mainline_udata *udata, struct vk_kern *kern_ptr)
{
	udata->kern_ptr = kern_ptr;
}
struct vk_kern *vk_kern_mainline_udata_get_kern(struct vk_kern_mainline_udata *udata)
{
	return udata->kern_ptr;
}
void vk_kern_mainline_udata_set_proc(struct vk_kern_mainline_udata *udata, struct vk_proc *proc_ptr)
{
	udata->proc_ptr = proc_ptr;
}
struct vk_proc *vk_kern_mainline_udata_get_proc(struct vk_kern_mainline_udata *udata)
{
	return udata->proc_ptr;
}
void vk_kern_mainline_udata_save_kern_hd(struct vk_kern_mainline_udata *udata)
{
	udata->kern_ptr->hd = udata->kern_hd;
}
void vk_kern_mainline_udata_open_kern_hd(struct vk_kern_mainline_udata *udata)
{
	udata->kern_hd = udata->kern_ptr->hd;
}
struct vk_heap *vk_kern_mainline_udata_get_kern_hd(struct vk_kern_mainline_udata *udata)
{
	return &udata->kern_hd;
}



void vk_kern_clear(struct vk_kern* kern_ptr) { memset(kern_ptr, 0, sizeof(*kern_ptr)); }

void vk_kern_set_shutdown_requested(struct vk_kern* kern_ptr) { kern_ptr->shutdown_requested = 1; }
void vk_kern_clear_shutdown_requested(struct vk_kern* kern_ptr) { kern_ptr->shutdown_requested = 0; }
int vk_kern_get_shutdown_requested(struct vk_kern* kern_ptr) { return kern_ptr->shutdown_requested; }

struct vk_heap* vk_kern_get_heap(struct vk_kern* kern_ptr) { return &kern_ptr->hd; }

struct vk_fd_table* vk_kern_get_fd_table(struct vk_kern* kern_ptr) { return kern_ptr->fd_table_ptr; }

void vk_kern_dump(struct vk_kern* kern_ptr, int clear)
{
	struct vk_pool_entry* entry_ptr;
	struct vk_proc* proc_ptr;
	struct vk_heap* heap_ptr;
	struct vk_proc_local* proc_local_ptr;
	int heap_entered;

	if (clear) {
		vk_tty_clear();
		vk_tty_reset();
	}

	entry_ptr = NULL;
	do {
		entry_ptr = vk_pool_next_entry(&kern_ptr->proc_pool, entry_ptr);
		if (entry_ptr != NULL) {
			proc_ptr = vk_kern_get_proc(kern_ptr, entry_ptr->entry_id);

			if (!vk_proc_is_zombie(proc_ptr)) {
				vk_proc_log("alive");
			} else {
				continue;
			}

			proc_local_ptr = vk_proc_get_local(proc_ptr);
			if (proc_local_ptr != NULL) {
				heap_ptr = vk_proc_get_heap(proc_ptr);
				heap_entered = vk_heap_entered(heap_ptr);
				if (!heap_entered) {
					vk_heap_enter(heap_ptr);
				}

				if (!vk_proc_local_is_zombie(proc_local_ptr)) {
					vk_proc_local_log("alive");
					vk_proc_local_dump_run_q(proc_local_ptr);
					vk_proc_local_dump_blocked_q(proc_local_ptr);
				} else {
					vk_proc_local_log("zombie");
				}

				if (!heap_entered) {
					vk_heap_exit(heap_ptr);
				}
			}
		}
	} while (entry_ptr != NULL);

	vk_fd_table_dump(kern_ptr->fd_table_ptr);
}

void vk_kern_sync_signal_handler(struct vk_kern* kern_ptr, int signo)
{
	switch (signo) {
		case SIGTERM:
			vk_kern_set_shutdown_requested(kern_ptr);
			break;
#ifdef SIGINFO
		case SIGINFO:
#endif
		case SIGUSR1:
			vk_kern_dump(kern_ptr, 1);
			break;
		default:
			break;
	}
}

void vk_kern_receive_signal(struct vk_kern* kern_ptr)
{
	int signo;

	signo = vk_signal_recv();
	if (signo != 0) {
		vk_kern_sync_signal_handler(kern_ptr, signo);
	}
}

void vk_kern_signal_handler(void* handler_udata, int jump, siginfo_t* siginfo_ptr, ucontext_t* uc_ptr)
{
	struct vk_kern* kern_ptr;
	kern_ptr = (struct vk_kern*)handler_udata;
	int rc;
	char buf[256];

	(void)kern_ptr;

	if (jump == 0) {
		/* system-level signals */
		switch (siginfo_ptr->si_signo) {
			case SIGINT:
			case SIGQUIT:
			case SIGSEGV:
				/* hard exit */
				rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
				if (rc == -1) {
					return;
				}
				buf[rc - 1] = '\n';
				fprintf(stderr, "Immediate exit due to signal %i: %s\n", siginfo_ptr->si_signo, buf);
				exit(0);
			case SIGTERM:
				/* soft exit */
				rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
				if (rc == -1) {
					return;
				}
				buf[rc - 1] = '\n';
				fprintf(stderr, "Exit request received via signal %i: %s\n", siginfo_ptr->si_signo,
					buf);
				vk_signal_send(siginfo_ptr->si_signo);
				break;
#ifdef SIGINFO
			case SIGINFO:
#endif
			case SIGUSR1:
				vk_signal_send(siginfo_ptr->si_signo);
				break;
		}
	}
}

void vk_kern_signal_jumper(void* handler_udata, siginfo_t* siginfo_ptr, ucontext_t* uc_ptr)
{
	struct vk_kern* kern_ptr;
	kern_ptr = (struct vk_kern*)handler_udata;
	int rc;
	char buf[256];

	/* system-level signals */

	rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
	if (rc == -1) {
		return;
	}
	buf[rc - 1] = '\n';
	fprintf(stderr, "Panic due to signal %i in kernel %p: %s\n", siginfo_ptr->si_signo, kern_ptr, buf);
	exit(1);
}

struct vk_proc* vk_kern_get_proc(struct vk_kern* kern_ptr, size_t i)
{
	struct vk_pool_entry* entry_ptr;
	entry_ptr = vk_pool_get_entry(&kern_ptr->proc_pool, i);
	if (entry_ptr == NULL) {
		return NULL;
	}
	return (struct vk_proc*)vk_stack_get_start(vk_heap_get_stack(vk_pool_entry_get_heap(entry_ptr)));
}

int vk_kern_proc_init(struct vk_pool_entry* entry_ptr, void* udata) { return 0; }
int vk_kern_proc_free(struct vk_pool_entry* entry_ptr, void* udata)
{
	struct vk_proc* proc_ptr;

	proc_ptr = (struct vk_proc*)vk_stack_get_start(vk_heap_get_stack(vk_pool_entry_get_heap(entry_ptr)));
	vk_proc_clear(proc_ptr);

	return 0;
}
int vk_kern_proc_deinit(struct vk_pool_entry* entry_ptr, void* udata) { return 0; }

struct vk_kern* vk_kern_alloc(struct vk_heap* hd_ptr)
{
	struct vk_kern* kern_ptr;
	int rc;
	int i;
	size_t kern_alignedlen;
	size_t fd_alignedlen;
	size_t pool_alignedlen;
	size_t alignedlen;
	size_t hugealignedlen;
	size_t entry_buffer_size;
	size_t object_buffer_size;

    /* Huge page tracker (AUTO mode by default). */
    struct vk_huge huge;
    vk_huge_init(&huge);

	/* allocations */
	rc = vk_safe_alignedlen(1, vk_kern_alloc_size(), &kern_alignedlen);
	if (rc == -1) {
		return NULL;
	}

	rc = vk_safe_alignedlen(1, vk_fd_table_alloc_size(VK_FD_MAX), &fd_alignedlen);
	if (rc == -1) {
		return NULL;
	}

	rc = vk_pool_needed_buffer_size(sizeof(struct vk_proc), VK_KERN_PROC_MAX, &entry_buffer_size,
					&object_buffer_size);
	if (rc == -1) {
		return NULL;
	}
	pool_alignedlen = entry_buffer_size + object_buffer_size;

	alignedlen = kern_alignedlen + fd_alignedlen + pool_alignedlen;

    /* Compute a hugepage-aligned length for HUGETLB attempt; default to base length. */
    if (vk_huge_aligned_len(&huge, 1, alignedlen, &hugealignedlen) == -1) {
        hugealignedlen = alignedlen;
    }

	vk_kdbgf("Allocating virtual kernel memory segment:\n"
		 "\tTotal: %zu:\n"
		 "\t\tkernel object: paged(%zu) = %zu\n"
		 "\t\tfd[%i] table: paged(%zu) = %zu\n"
		 "\t\t\ttable object: %zu\n"
		 "\t\t\t\tpollfd[%i]: %zu*%i = %zu\n"
		 "\t\t\tfd[%i]: %zu*%i = %zu\n"
		 "\t\tproc[%i] table: %zu\n"
		 "\t\t\ttable index: paged(%zu)*%i = %zu\n"
		 "\t\t\ttable entry: paged(%zu)*%i = %zu\n",
		 alignedlen,
	                vk_kern_alloc_size(), kern_alignedlen,
			VK_FD_MAX,
                                vk_fd_table_alloc_size(VK_FD_MAX), fd_alignedlen,
				vk_fd_table_object_size(),
					VK_FD_MAX, sizeof (struct pollfd), VK_FD_MAX, sizeof (struct pollfd) * VK_FD_MAX,
		                VK_FD_MAX, vk_fd_table_entry_size(), VK_FD_MAX, vk_fd_table_entry_size() * VK_FD_MAX,
		        VK_KERN_PROC_MAX,
				pool_alignedlen,
				sizeof (struct vk_pool_entry), VK_KERN_PROC_MAX, entry_buffer_size,
				sizeof (struct vk_proc), VK_KERN_PROC_MAX, object_buffer_size);

    /* Attempt MAP_HUGETLB only if likely to succeed; otherwise regular map. */
    int base_flags = MAP_ANON | MAP_PRIVATE;
    int attempt_huge = vk_huge_should_try(&huge, hugealignedlen);
    if (attempt_huge) {
        int flags = base_flags | vk_huge_flags(&huge);
        rc = vk_heap_map(hd_ptr, NULL, hugealignedlen, 0, flags, -1, 0, 1);
        if (rc != -1) {
            vk_huge_on_success(&huge, hugealignedlen);
        } else if (errno == ENOMEM || errno == EINVAL || errno == EAGAIN || errno == EPERM) {
            vk_huge_on_failure(&huge, errno);
            rc = vk_heap_map(hd_ptr, NULL, alignedlen, 0, base_flags, -1, 0, 1);
        }
    } else {
        rc = vk_heap_map(hd_ptr, NULL, alignedlen, 0, base_flags, -1, 0, 1);
    }
    if (rc == -1) {
        vk_klogf("failed to allocate %zu bytes for the virtual kernel memory segment.\n", alignedlen);
        vk_kperror("vk_heap_map");
        return NULL;
    }

	kern_ptr = vk_stack_push(vk_heap_get_stack(hd_ptr), 1, kern_alignedlen);
	if (kern_ptr == NULL) {
		return NULL;
	}
	kern_ptr->hd = *hd_ptr;

	kern_ptr->fd_table_ptr = vk_stack_push(vk_heap_get_stack(hd_ptr), 1, fd_alignedlen);
	if (kern_ptr->fd_table_ptr == NULL) {
		return NULL;
	}

	kern_ptr->fd_table_ptr->size = VK_FD_MAX;

	kern_ptr->pool_buffer = vk_stack_push(vk_heap_get_stack(hd_ptr), 1, pool_alignedlen);
	if (kern_ptr->pool_buffer == NULL) {
		return NULL;
	}

	/* initializations */
	rc = vk_pool_init(&kern_ptr->proc_pool, sizeof(struct vk_proc), VK_KERN_PROC_MAX, kern_ptr->pool_buffer,
			  pool_alignedlen, vk_kern_proc_init, NULL, vk_kern_proc_free, NULL, vk_kern_proc_deinit, NULL,
			  1);
	if (rc == -1) {
		return NULL;
	}
	for (i = 0; i < VK_KERN_PROC_MAX; i++) {
		vk_kern_get_proc(kern_ptr, i)->proc_id = i;
	}

	rc = vk_signal_init();
	if (rc == -1) {
		return NULL;
	}

	/* create per-worker isolate */
	kern_ptr->iso_initialized = 0;
	if (vk_isolate_init(&kern_ptr->iso) == 0) {
		kern_ptr->iso_initialized = 1;
		/* default scheduler callback updated per-proc before dispatch */
		vk_isolate_set_scheduler(&kern_ptr->iso, vk_isolate_scheduler_cb, NULL);
	}
	vk_signal_set_handler(vk_kern_signal_handler, (void*)kern_ptr);
	vk_signal_set_jumper(vk_kern_signal_jumper, (void*)kern_ptr);

	SLIST_INIT(&kern_ptr->run_procs);
	SLIST_INIT(&kern_ptr->blocked_procs);
	SLIST_INIT(&kern_ptr->deferred_procs);

	return kern_ptr;
}

size_t vk_kern_alloc_size() { return sizeof(struct vk_kern); }

struct vk_proc* vk_kern_alloc_proc(struct vk_kern* kern_ptr, struct vk_pool* pool_ptr)
{
	struct vk_pool_entry* entry_ptr;
	struct vk_proc* proc_ptr;

	entry_ptr = vk_pool_alloc_entry(&kern_ptr->proc_pool);
	if (entry_ptr == NULL) {
		return NULL;
	}

	proc_ptr = vk_stack_get_start(vk_heap_get_stack(vk_pool_entry_get_heap(entry_ptr)));

	vk_proc_dbg("allocated in kernel");

	return proc_ptr;
}

void vk_kern_free_proc(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	vk_pool_free_entry(&kern_ptr->proc_pool, vk_pool_get_entry(&kern_ptr->proc_pool, proc_ptr->proc_id));
}

struct vk_proc* vk_kern_first_run(struct vk_kern* kern_ptr) { return SLIST_FIRST(&kern_ptr->run_procs); }

struct vk_proc* vk_kern_first_blocked(struct vk_kern* kern_ptr) { return SLIST_FIRST(&kern_ptr->blocked_procs); }

struct vk_proc* vk_kern_first_deferred(struct vk_kern* kern_ptr)
{
	return SLIST_FIRST(&kern_ptr->deferred_procs);
}

void vk_kern_enqueue_run(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	vk_proc_dbg("enqueuing to run");
	if (!proc_ptr->run_qed) {
		proc_ptr->run_qed = 1;
		SLIST_INSERT_HEAD(&kern_ptr->run_procs, proc_ptr, run_list_elem);
		vk_proc_dbg("enqueued to run");
	}
}

void vk_kern_enqueue_blocked(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	vk_proc_dbg("enqueuing to block");
	if (!proc_ptr->blocked_qed) {
		proc_ptr->blocked_qed = 1;
		SLIST_INSERT_HEAD(&kern_ptr->blocked_procs, proc_ptr, blocked_list_elem);
		vk_proc_dbg("enqueued to block");
	}
}

void vk_kern_enqueue_deferred(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	vk_proc_dbg("enqueuing to defer");
	if (!proc_ptr->deferred_qed) {
		proc_ptr->deferred_qed = 1;
		SLIST_INSERT_HEAD(&kern_ptr->deferred_procs, proc_ptr, deferred_list_elem);
		vk_proc_dbg("enqueued to defer");
	}
}

void vk_kern_drop_run(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	vk_proc_dbg("dropping from run queue");
	if (proc_ptr->run_qed) {
		proc_ptr->run_qed = 0;
		SLIST_REMOVE(&kern_ptr->run_procs, proc_ptr, vk_proc, run_list_elem);
		vk_proc_dbg("dropped from run queue");
	}
}

void vk_kern_drop_blocked(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	vk_proc_dbg("dropping from block queue");
	if (proc_ptr->blocked_qed) {
		proc_ptr->blocked_qed = 0;
		SLIST_REMOVE(&kern_ptr->blocked_procs, proc_ptr, vk_proc, blocked_list_elem);
		vk_proc_dbg("dropped from block queue");
	}
}

void vk_kern_drop_deferred(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	vk_proc_dbg("dropping from defer queue");
	if (proc_ptr->deferred_qed) {
		proc_ptr->deferred_qed = 0;
		SLIST_REMOVE(&kern_ptr->deferred_procs, proc_ptr, vk_proc, deferred_list_elem);
		vk_proc_dbg("dropped from defer queue");
	}
}

struct vk_proc* vk_kern_dequeue_run(struct vk_kern* kern_ptr)
{
	struct vk_proc* proc_ptr;

	if (SLIST_EMPTY(&kern_ptr->run_procs)) {
		return NULL;
	}

	proc_ptr = SLIST_FIRST(&kern_ptr->run_procs);
	SLIST_REMOVE_HEAD(&kern_ptr->run_procs, run_list_elem);
	proc_ptr->run_qed = 0;
	vk_proc_dbg("dequeued to run");

	return proc_ptr;
}

struct vk_proc* vk_kern_dequeue_blocked(struct vk_kern* kern_ptr)
{
	struct vk_proc* proc_ptr;

	if (SLIST_EMPTY(&kern_ptr->blocked_procs)) {
		return NULL;
	}

	proc_ptr = SLIST_FIRST(&kern_ptr->blocked_procs);
	SLIST_REMOVE_HEAD(&kern_ptr->blocked_procs, blocked_list_elem);
	proc_ptr->blocked_qed = 0;
	vk_proc_dbg("dequeued to unblock");

	return proc_ptr;
}

struct vk_proc* vk_kern_dequeue_deferred(struct vk_kern* kern_ptr)
{
	struct vk_proc* proc_ptr;

	if (SLIST_EMPTY(&kern_ptr->deferred_procs)) {
		return NULL;
	}

	proc_ptr = SLIST_FIRST(&kern_ptr->deferred_procs);
	SLIST_REMOVE_HEAD(&kern_ptr->deferred_procs, deferred_list_elem);
	proc_ptr->deferred_qed = 0;
	vk_proc_dbg("dequeued to process deferred");

	return proc_ptr;
}

void vk_kern_flush_proc_queues(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	struct vk_proc_local* proc_local_ptr = vk_proc_get_local(proc_ptr);
	if (vk_proc_local_get_run(proc_local_ptr)) {
		vk_kern_enqueue_run(kern_ptr, proc_ptr);
	} else {
		vk_kern_drop_run(kern_ptr, proc_ptr);
	}

	if (vk_proc_local_get_deferred(proc_local_ptr)) {
		vk_kern_enqueue_deferred(kern_ptr, proc_ptr);
	} else {
		vk_kern_drop_deferred(kern_ptr, proc_ptr);
	}

	if (vk_proc_local_get_blocked(proc_local_ptr)) {
		vk_kern_enqueue_blocked(kern_ptr, proc_ptr);
	} else {
		vk_kern_drop_blocked(kern_ptr, proc_ptr);
	}
}

int vk_kern_pending(struct vk_kern* kern_ptr)
{
	return !(SLIST_EMPTY(&kern_ptr->run_procs) && SLIST_EMPTY(&kern_ptr->blocked_procs) &&
	         SLIST_EMPTY(&kern_ptr->deferred_procs));
}

int vk_kern_prepoll(struct vk_kern* kern_ptr)
{
        struct vk_proc* proc_ptr;
        struct vk_proc* proc_cursor_ptr;

        if (!vk_kern_get_shutdown_requested(kern_ptr)) {
                return 0;
        }

        /* Wake any processes blocked in the poller by raising EINTR. */
        SLIST_FOREACH_SAFE(proc_ptr, &kern_ptr->blocked_procs, blocked_list_elem, proc_cursor_ptr)
        {
                struct vk_proc_local* proc_local_ptr;
                struct vk_socket* socket_ptr;
                struct vk_socket* socket_cursor_ptr;
                struct vk_thread* that;

                proc_local_ptr = vk_proc_get_local(proc_ptr);
                /* mark process runnable so flush can promote it */
                vk_proc_set_run(proc_ptr, 1);
                socket_ptr = vk_proc_local_first_blocked(proc_local_ptr);
                while (socket_ptr != NULL) {
                        socket_cursor_ptr = SLIST_NEXT(socket_ptr, blocked_q_elem);
                        that = vk_block_get_vk(vk_socket_get_block(socket_ptr));
                        vk_vectoring_set_rx_blocked(vk_socket_get_rx_vectoring(socket_ptr), 0);
                        vk_vectoring_set_tx_blocked(vk_socket_get_tx_vectoring(socket_ptr), 0);
                        vk_proc_local_drop_blocked(proc_local_ptr, socket_ptr);
                        vk_block_set_vk(vk_socket_get_block(socket_ptr), NULL);
                        if (that) {
                                /* add thread to run queue so it can observe EINTR */
                                vk_proc_local_enqueue_run(proc_local_ptr, that);
                                vk_raise_at(that, EINTR);
                        }
                        socket_ptr = socket_cursor_ptr;
                }

                vk_kern_flush_proc_queues(kern_ptr, proc_ptr);
        }

        return 0;
}

void vk_proc_execute_mainline(void* mainline_udata)
{
	int rc;
	struct vk_kern* kern_ptr;
	struct vk_proc* proc_ptr;

	kern_ptr = ((struct vk_kern_mainline_udata*)mainline_udata)->kern_ptr;
	proc_ptr = ((struct vk_kern_mainline_udata*)mainline_udata)->proc_ptr;
	rc = vk_proc_execute(proc_ptr, kern_ptr);
	if (rc == -1) {
		kern_ptr->rc = -1;
	}

	vk_signal_set_jumper(vk_kern_signal_jumper, (void*)kern_ptr);

	kern_ptr->rc = 0;
}

vk_isolate_t *vk_kern_get_isolate(struct vk_kern* kern_ptr)
{
    return (kern_ptr && kern_ptr->iso_initialized) ? &kern_ptr->iso : NULL;
}

void vk_kern_set_isolate_user_state(struct vk_kern* kern_ptr, struct vk_proc_local* pl)
{
    if (kern_ptr && kern_ptr->iso_initialized) {
        vk_isolate_set_scheduler(&kern_ptr->iso, vk_isolate_scheduler_cb, pl);
    }
}

void vk_proc_execute_jumper(void* jumper_udata, siginfo_t* siginfo_ptr, ucontext_t* uc_ptr)
{
	struct vk_proc* proc_ptr;
	struct vk_kern* kern_ptr;
	char buf[256];
	int rc;

	/*
	 * Several entrypoints:
	 * 1. the kernel directly (while kernel still accessible)
	 * 2. the signal handler (while kernel still accessible)
	 * 3. the signal handler (while kernel is NOT accessible)
	 *
	 * In entrypoint #3, both the kernel heap and the process heap may be masked
	 * (kernel: protected; process: PROT_NONE for isolated procs). Re-enter both
	 * heaps so we can safely touch proc-local state and resume execution.
	 */
    // We may arrive here from any signal (SIGSYS, SIGSEGV, etc.).
    // First re-enter the kernel heap before dereferencing anything inside it.
    kern_ptr = vk_kern_mainline_udata_get_kern(vk_signal_get_mainline_udata());
    vk_heap_enter(vk_kern_mainline_udata_get_kern_hd(vk_signal_get_mainline_udata()));
    // Now that kernel heap is accessible, open the isolate gate (safe for all traps).
    if (kern_ptr && vk_kern_get_isolate(kern_ptr)) {
        vk_isolate_on_signal(vk_kern_get_isolate(kern_ptr));
    }

    proc_ptr = (struct vk_proc*)jumper_udata;
    /* ensure the active process heap is accessible before touching proc-local */
    vk_heap_enter(vk_proc_get_heap(proc_ptr));

    // Diagnostics: print heap protections to help pinpoint masking state
    DBG("  kern heap prot=%i\n", vk_kern_mainline_udata_get_kern_hd(vk_signal_get_mainline_udata())->mapping.prot);
    DBG("  proc heap prot=%i\n", vk_proc_get_heap(proc_ptr)->mapping.prot);

	vk_proc_local_set_siginfo(vk_proc_get_local(proc_ptr), *siginfo_ptr);
	vk_proc_local_set_uc(vk_proc_get_local(proc_ptr), uc_ptr);

	rc = vk_signal_get_siginfo_str(vk_proc_local_get_siginfo(vk_proc_get_local(proc_ptr)), buf, sizeof(buf) - 1);
	if (rc != -1) {
		DBG("siginfo_ptr = %s\n", buf);
		if (siginfo_ptr->si_addr != NULL) {
			DBG("  fault address: %p\n", siginfo_ptr->si_addr);
			// report whether address is in the vk_kern heap, or in the vk_proc heap
			if (vk_heap_addr_in_heap(vk_kern_mainline_udata_get_kern_hd(vk_signal_get_mainline_udata()), siginfo_ptr->si_addr)) {
				DBG("  (in vk_kern heap)\n");
			} else if (vk_heap_addr_in_heap(vk_proc_get_heap(proc_ptr), siginfo_ptr->si_addr)) {
				DBG("  (in the active vk_proc %zu heap)\n", proc_ptr->proc_id);
			} else {
				// look through all procs in the kernel pool to see if it is in any of their heaps
				struct vk_pool_entry* entry_ptr = NULL;
				if (vk_proc_get_pool(proc_ptr) != NULL) {
					while ((entry_ptr = vk_pool_next_entry(vk_proc_get_pool(proc_ptr), entry_ptr)) != NULL) {
						if (vk_heap_addr_in_heap(vk_pool_entry_get_heap(entry_ptr), siginfo_ptr->si_addr)) {
							DBG("  (in the inactive vk_proc %zu heap)\n", vk_pool_entry_get_id(entry_ptr));
							break;
						}
					}
					if (entry_ptr == NULL) {
						DBG("  (not in vk_kern nor vk_proc heap)\n");
					}
				} else {
					DBG("  (not in vk_kern and no vk_proc pool)\n");
				}
			}
		}
	}

	vk_proc_execute_mainline(vk_signal_get_mainline_udata());
}

int vk_kern_execute_proc(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	int rc;
	struct vk_kern_mainline_udata mainline_udata;

	vk_signal_set_jumper(vk_proc_execute_jumper, (void*)proc_ptr);
	mainline_udata.kern_ptr = kern_ptr;
	mainline_udata.proc_ptr = proc_ptr;
	vk_signal_set_mainline(vk_proc_execute_mainline, &mainline_udata);

	rc = vk_signal_setjmp();
	if (rc == -1) {
		return -1;
	}

	/* deferred error from jumper */
	rc = kern_ptr->rc;
	if (rc == -1) {
		return -1;
	}

	vk_signal_set_jumper(vk_kern_signal_jumper, (void*)kern_ptr);

	return 0;
}

int vk_kern_dispatch_proc(struct vk_kern* kern_ptr, struct vk_proc* proc_ptr)
{
	int rc;
	struct vk_pool* pool_ptr;

	vk_proc_dbg("dispatch proc");

	vk_proc_dbg("entering process heap");
	rc = vk_heap_enter(vk_proc_get_heap(proc_ptr));
	if (rc == -1) {
		return -1;
	}

	rc = vk_kern_execute_proc(kern_ptr, proc_ptr);
	if (rc == -1) {
		return -1;
	}

	vk_kern_flush_proc_queues(kern_ptr, proc_ptr);

	if (vk_proc_is_zombie(proc_ptr)) {
		vk_fd_table_prepoll_zombie(kern_ptr->fd_table_ptr, proc_ptr);
		pool_ptr = vk_proc_get_pool(proc_ptr);
		if (pool_ptr == NULL) {
			vk_proc_dbg("exiting zombie process heap (by freeing heap mapping)");
			rc = vk_proc_free(proc_ptr);
			if (rc == -1) {
				return -1;
			}
		} else {
			rc = vk_proc_free_from_pool(proc_ptr, pool_ptr);
			if (rc == -1) {
				return -1;
			}

			vk_proc_dbg("exiting zombie process heap (after freeing from pool)");
			rc = vk_heap_exit(vk_proc_get_heap(proc_ptr));
			if (rc == -1) {
				return -1;
			}
		}
		vk_kern_free_proc(kern_ptr, proc_ptr);
	} else {
		if (vk_proc_get_isolated(proc_ptr)) {
			vk_proc_dbg("exiting isolated process heap");
			rc = vk_heap_exit(vk_proc_get_heap(proc_ptr));
			if (rc == -1) {
				return -1;
			}
		} else {
			vk_proc_dbg("not exiting non-isolated process heap");
		}
	}

	return 0;
}

int vk_kern_postpoll(struct vk_kern* kern_ptr)
{
	int rc;
	struct vk_fd* fd_ptr;
	struct vk_proc* proc_ptr;

	/* dispatch new poll events */
	while ((fd_ptr = vk_fd_table_dequeue_fresh(kern_ptr->fd_table_ptr))) {
		/* dispatch process by enqueuing to run */
		vk_kern_enqueue_run(kern_ptr, vk_kern_get_proc(kern_ptr, vk_fd_get_proc_id(fd_ptr)));
	}

	/* dispatch new runnable procs */
	while ((proc_ptr = vk_kern_dequeue_run(kern_ptr))) {
		vk_kern_receive_signal(kern_ptr);
		rc = vk_kern_dispatch_proc(kern_ptr, proc_ptr);
		if (rc == -1) {
			return -1;
		}
		vk_kern_receive_signal(kern_ptr);
	}

	/* process aggregated I/O for deferred procs */
	while ((proc_ptr = vk_kern_dequeue_deferred(kern_ptr))) {
		vk_kern_receive_signal(kern_ptr);
		rc = vk_kern_dispatch_proc(kern_ptr, proc_ptr);
		if (rc == -1) {
			return -1;
		}
		vk_kern_receive_signal(kern_ptr);
		/* If the proc immediately re-enqueued itself to the deferred queue,
		 * avoid spinning by processing it again in the same pass. We'll pick
		 * it up on the next loop iteration after poll. */
		if (proc_ptr->deferred_qed) {
			break;
		}
	}

	/* newly runnable work surfaced by deferred processing */
	while ((proc_ptr = vk_kern_dequeue_run(kern_ptr))) {
		vk_kern_receive_signal(kern_ptr);
		rc = vk_kern_dispatch_proc(kern_ptr, proc_ptr);
		if (rc == -1) {
			return -1;
		}
		vk_kern_receive_signal(kern_ptr);
	}

	return 0;
}

int vk_kern_poll(struct vk_kern* kern_ptr)
{
	int rc;

	rc = vk_fd_table_wait(kern_ptr->fd_table_ptr, kern_ptr);
	if (rc == -1) {
		return -1;
	}

	return 0;
}

int vk_kern_loop(struct vk_kern* kern_ptr)
{
	int rc;

	DBG("entering vk_kern_loop\n");

	DBG("vk_kern_postpoll\n");
	rc = vk_kern_postpoll(kern_ptr);
	if (rc == -1) {
		return -1;
	}
	do {

		DBG("vk_kern_prepoll\n");
		rc = vk_kern_prepoll(kern_ptr);
		if (rc == -1) {
			return -1;
		}

		DBG("vk_kern_poll\n");
		rc = vk_kern_poll(kern_ptr);
		if (rc == -1) {
			return -1;
		}

		DBG("vk_kern_postpoll\n");
		rc = vk_kern_postpoll(kern_ptr);
		if (rc == -1) {
			return -1;
		}
	} while (vk_kern_pending(kern_ptr));

	DBG("vk_kern_dump\n");
	vk_kern_dump(kern_ptr, 0);

	DBG("exiting vk_kern_loop\n");

	return 0;
}
