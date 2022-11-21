#ifndef VK_PROC_S_H
#define VK_PROC_S_H

#include <sys/queue.h>

#include "vk_heap_s.h"
#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_poll_s.h"
#include "vk_signal.h"
#include "vk_proc.h"
#include "vk_proc_local.h"

struct vk_proc {
    /* dispatching */
    size_t proc_id; /* kern-rw */
    size_t pool_entry_id; /* kern-read */
    int rc; /* kern-rw */

    /* scheduling */
    int run_qed;     /* kern-read */
    int blocked_qed; /* kern-read */
    SLIST_ENTRY(vk_proc) run_list_elem;     /* kern-rw */
    SLIST_ENTRY(vk_proc) blocked_list_elem; /* kern-rw */

    /* polling */
    struct pollfd fds[VK_PROC_MAX_EVENTS]; /* kern-rw */
    int nfds; /* kern-read */

    struct vk_proc_local *local_ptr;

    /* memory */
    struct vk_heap heap; /* all-rw */
};

#endif