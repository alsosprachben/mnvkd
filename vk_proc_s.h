#ifndef VK_PROC_S_H
#define VK_PROC_S_H

#include <sys/queue.h>

#include "vk_heap_s.h"
#include "vk_state_s.h"
#include "vk_poll_s.h"
#include "vk_socket.h"
#include "vk_socket_s.h"

struct vk_proc {
    struct vk_heap_descriptor heap;
    SLIST_HEAD(run_q_head, that) run_q;
    SLIST_HEAD(blocked_q_head, vk_socket) blocked_q;
    struct io_future events[VK_PROC_MAX_EVENTS];
    struct pollfd fds[VK_PROC_MAX_EVENTS];
    int nfds;
};

#endif