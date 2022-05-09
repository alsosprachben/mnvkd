#ifndef VK_PROC_S_H
#define VK_PROC_S_H

#include <sys/queue.h>

#include "vk_heap_s.h"
#include "vk_state.h"

struct vk_proc {
    struct vk_heap_descriptor heap;
    SLIST_HEAD(run_q_head, that) run_q;
    SLIST_HEAD(blocked_q_head, that) blocked_q;
};

#endif