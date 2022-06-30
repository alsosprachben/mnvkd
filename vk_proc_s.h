#ifndef VK_PROC_S_H
#define VK_PROC_S_H

#include <sys/queue.h>

#include "vk_heap_s.h"
#include "vk_thread.h"
#include "vk_socket.h"
#include "vk_poll_s.h"

struct vk_kern;
struct vk_proc {
    struct vk_kern *kern_ptr;
    size_t proc_id;
    int run;
    int run_qed;
    int blocked;
    int blocked_qed;
    struct vk_heap_descriptor heap;
    SLIST_HEAD(run_q_head, vk_thread) run_q;
    SLIST_HEAD(blocked_q_head, vk_socket) blocked_q;
    struct io_future events[VK_PROC_MAX_EVENTS];
    struct pollfd fds[VK_PROC_MAX_EVENTS];
    int nfds;
    SLIST_ENTRY(vk_proc) free_list_elem;
    SLIST_ENTRY(vk_proc) run_list_elem;
    SLIST_ENTRY(vk_proc) blocked_list_elem;
};

#endif