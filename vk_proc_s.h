#ifndef VK_PROC_S_H
#define VK_PROC_S_H

#include <sys/queue.h>

#include "vk_heap_s.h"
#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_poll_s.h"
#include "vk_signal.h"
#include "vk_proc.h"

struct vk_proc {
    size_t proc_id;
    int run;
    int run_qed;
    int blocked;
    int blocked_qed;
    struct vk_heap heap;
    size_t pool_entry_id;
    SLIST_HEAD(run_q_head, vk_thread) run_q;
    SLIST_HEAD(blocked_q_head, vk_socket) blocked_q;
    struct io_future events[VK_PROC_MAX_EVENTS];
    struct pollfd fds[VK_PROC_MAX_EVENTS];
    int nfds;
    SLIST_ENTRY(vk_proc) free_list_elem;
    SLIST_ENTRY(vk_proc) run_list_elem;
    SLIST_ENTRY(vk_proc) blocked_list_elem;
    struct vk_thread *running_cr;
    struct vk_thread *supervisor_cr;
    siginfo_t siginfo;
    ucontext_t *uc_ptr;
    int rc;
};

#endif