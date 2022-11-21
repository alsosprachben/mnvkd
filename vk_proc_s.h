#ifndef VK_PROC_S_H
#define VK_PROC_S_H

#include <sys/queue.h>

#include "vk_heap_s.h"
#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_poll_s.h"
#include "vk_signal.h"
#include "vk_proc.h"

/* dependency encapsulation */
struct vk_proc_local {
    size_t proc_id;
    
    /* scheduling */
    int run;         /* proc-write */
    int blocked;     /* proc-write */
    SLIST_HEAD(run_q_head, vk_thread) run_q; /* proc-rw */
    SLIST_HEAD(blocked_q_head, vk_socket) blocked_q; /* proc-rw */

    /* polling */
    struct io_future events[VK_PROC_MAX_EVENTS]; /* proc-write */
    int nfds; /* proc-write */

    /* signal handling */
    struct vk_thread *running_cr; /* proc-rw */
    struct vk_thread *supervisor_cr; /* proc-rw */
    siginfo_t siginfo; /* proc-read */
    ucontext_t *uc_ptr; /* proc-read */

    /* memory */
    struct vk_stack stack; /* proc-rw */
};

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