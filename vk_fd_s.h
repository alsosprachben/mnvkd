#ifndef VK_FD_S_H
#define VK_FD_S_H

#include "vk_queue.h"
#include "vk_io_future_s.h"


/*
 * Lifecycle after executing process:
 * 1. vk_proc_local_prepoll(): for each socket:
 *   a. vk_fd_table_prepoll() marks dirty:
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
	int fd;
	size_t proc_id;
	int allocated;
	int error;
	int dirty_qed; /* to register */
	int fresh_qed; /* to dispatch */
	SLIST_ENTRY(vk_fd) dirty_list_elem; /* element in dirty list */
	SLIST_ENTRY(vk_fd) fresh_list_elem; /* element in fresh list */
	struct vk_io_future ioft_post; /* state registered or polled: physical, posterior */
	struct vk_io_future ioft_pre;  /* state to register:          logical,  prior */
	struct vk_io_future ioft_ret;  /* state to dispatch:          logical,  posterior */
};


#endif
