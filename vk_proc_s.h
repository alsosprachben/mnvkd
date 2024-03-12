#ifndef VK_PROC_S_H
#define VK_PROC_S_H

#include "vk_queue.h"
#include "vk_heap_s.h"
#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_signal.h"
#include "vk_pool.h"
#include "vk_fd.h"
#include "vk_fd_s.h"
#include "vk_proc.h"
#include "vk_proc_local.h"

struct vk_proc {
    /* dispatching */
    size_t proc_id; /* kern-rw */
    size_t pool_entry_id; /* kern-read */
    struct vk_pool *pool_ptr; /* kern-read */
    struct vk_pool_entry *entry_ptr;
    int rc; /* kern-rw */

    /* scheduling */
    int run_qed;     /* kern-read */
    int blocked_qed; /* kern-read */
    SLIST_ENTRY(vk_proc) run_list_elem;     /* kern-rw; head on `struct vk_kern` member `run_procs` */
    SLIST_ENTRY(vk_proc) blocked_list_elem; /* kern-rw; head on `struct vk_kern` member `blocked_procs` */

    SLIST_HEAD(allocated_fds_head, vk_fd) allocated_fds;

    struct vk_proc_local *local_ptr;

    /* memory */
    struct vk_heap heap; /* all-rw */
    int privileged; /* whether to not protect the kernel when running, for processes that need to spawn other processes */
};

#endif
