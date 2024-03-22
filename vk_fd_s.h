#ifndef VK_FD_S_H
#define VK_FD_S_H

#include "vk_queue.h"
#include "vk_io_future_s.h"
#if defined(VK_USE_GETEVENTS)
#include <linux/aio_abi.h>
#endif


/*
 * Lifecycle after executing process:
 * 1. vk_proc_local_prepoll(): for each socket:
 *   a. vk_fd_table_prepoll_blocked_socket() marks dirty:
 *   b. vk_fd_table_enqueue_dirty() prior to:
 * 2. fd_kern_poll(): vk_fd_table_poll()
 *   a. registers dirty vk_fd_table_drop_dirty() (only dropping if system holds registration)
 *   b. returning fresh vk_fd_table_enqueue_fresh()
 *     i. if fresh not already dropped from dirty queue, drop now
 * 3. vk_kern_postpoll():
 *   a. for each fresh vk_fd:
 *     i. for the ioft_pre's socket: vk_fd_table_postpoll() 
 *     ii. for the proc_id: vk_kern_enqueue_run()
 *     iii. vk_fd_table_drop_fresh()
 *   b. for each proc in kern run queue:
 *     i. dispatch proc vk_kern_dispatch_proc() wrapped in vk_kern_receive_signal()
 *       A. vk_proc_local_postpoll(): for each socket:
 *          I.  vk_fd_table_postpoll() checks for event match, and dispatches:
 *          II. vk_proc_local_retry_socket() to retry the I/O op
 *       B. execute process, then back to 1
 * 
 * change:
 *  - vk_proc_execute -> needs reference to vk_fd_table
 *  - vk_proc_prepoll -> vk_proc_local_prepoll
 *  - vk_proc_postpoll -> vk_proc_local_postpoll
 *  - vk_kern_prepoll -> no-op
 *  - vk_kern_postpoll -> code
 *  - vk_kern_poll() -> code
 */

struct vk_fd {
    /* identifiers */
	int fd; /* file descriptor, both the physical and logical ID */
	size_t proc_id; /* logical process ID (only one physical process: `getpid()`) -- the `struct vk_proc` and `struct vk_proc_local` it is attached to */
	/* There is a 1:1 relationship between `fd` and `proc_id`.
	 * Therefore, an FD can only be attached to a single logical process.
	 * Use `socketpair()` or `pipe2()` to communicate between protected logical or physical processes on the same system.
	 */

	/* physical state */
	int error;       /* `errno` from last physical I/O operation */
	int closed;      /* physical FD has been closed */
	int rx_shutdown; /* whether read side of physical socket is shut down */
	int tx_shutdown; /* whether write side of physical socket is shut down */
	int added;       /* whether edge-triggered event has been added to the physical FD */

    /* iterable set membership */
	int allocated; /* whether attached to a process -- see `proc_id` for which process */
	int dirty_qed; /* to register -- `ioft_pre` to `ioft_post` */
	int fresh_qed; /* to dispatch -- `ioft_post` to `ioft_ret` */
    SLIST_ENTRY(vk_fd) allocated_list_elem; /* element in tracked list -- head on `struct vk_proc` at `allocated_fds` */
	SLIST_ENTRY(vk_fd) dirty_list_elem;     /* element in dirty   list -- head on `struct vk_fd_table` at `dirty_fds` */
	SLIST_ENTRY(vk_fd) fresh_list_elem;     /* element in fresh   list -- head on `struct vk_fd_table` at `fresh_fds` */

	/* registration lifecycle --  pre and ret, the logical state, is also represented in `struct vk_block` in `struct vk_socket` */
	struct vk_io_future ioft_post; /* state registered or polled: physical, posterior -- in poller */
	struct vk_io_future ioft_pre;  /* state to register:          logical,  prior     -- also via `struct vk_block` */
	struct vk_io_future ioft_ret;  /* state to dispatch:          logical,  posterior -- also via `struct vk_block` */
#if defined(VK_USE_GETEVENTS)
    struct iocb iocb;
#endif
};


#endif
