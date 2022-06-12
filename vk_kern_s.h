#ifndef VK_KERN_S_H
#define VK_KERN_S_H

#include <sys/queue.h>
#include <sys/poll.h>

#include "vk_proc.h"
#include "vk_proc_s.h"

#include "vk_heap.h"

#define VK_KERN_PAGE_COUNT 1024
#define VK_KERN_PROC_MAX 1024

struct vk_kern_event_index {
    size_t proc_id;
    size_t event_start_pos;
    int nfds;
};

struct vk_kern {
    struct vk_heap_descriptor *hd_ptr;
    // processes
    struct vk_proc proc_table[VK_KERN_PROC_MAX];
    size_t proc_count;

    // pull events for each process
    struct pollfd events[VK_KERN_PROC_MAX * VK_PROC_MAX_EVENTS];
    int nfds;

    // process index into poll events
    struct vk_kern_event_index event_index[VK_KERN_PROC_MAX];
    // count of event_index entries
    size_t event_index_count;
    // stack pointer into the next free slot in the poll events
    size_t event_proc_next_pos;

    SLIST_HEAD(free_procs_head,    vk_proc)    free_procs;
    SLIST_HEAD(run_procs_head,     vk_proc)     run_procs;
    SLIST_HEAD(blocked_procs_head, vk_proc) blocked_procs;
};

#endif