#ifndef VK_KERN_S_H
#define VK_KERN_S_H

#include <sys/queue.h>
#include <sys/poll.h>

#include "vk_proc.h"
#include "vk_proc_s.h"

#include "vk_heap.h"

#define VK_KERN_PAGE_COUNT 1024
#define VK_KERN_PROC_MAX 1024

struct vk_kern {
    struct vk_heap_descriptor *hd_ptr;
    // processes
    struct vk_proc proc_table[VK_KERN_PROC_MAX];
    size_t proc_count;

    // pull events for each process
    struct pollfd events[VK_KERN_PROC_MAX * VK_PROC_MAX_EVENTS];
    int nfds;

    // process index into poll events
    size_t event_proc_start_pos[VK_KERN_PROC_MAX];

    SLIST_HEAD(free_procs_head, vk_proc) free_procs;
};

#endif