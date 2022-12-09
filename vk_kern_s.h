#ifndef VK_KERN_S_H
#define VK_KERN_S_H

#include <sys/poll.h>

#include "vk_queue.h"

#include "vk_proc.h"
#include "vk_proc_s.h"
#include "vk_pool_s.h"

#include "vk_heap.h"
#include "vk_fd.h"
#include "vk_fd_s.h"

#define VK_KERN_PROC_MAX 16384
#define VK_FD_MAX 16384

struct vk_kern_event_index {
    size_t proc_id;
    size_t event_start_pos;
    int nfds;
};

struct vk_kern {
    struct vk_heap *hd_ptr;
    // processes
    struct vk_pool proc_pool;
    size_t proc_count;

    // poll events for each process
    struct pollfd events[VK_KERN_PROC_MAX * VK_PROC_MAX_EVENTS];
    int nfds;
    // stack pointer into the next free slot in the poll events
    size_t event_proc_next_pos;

    // process index into poll events
    struct vk_kern_event_index event_index[VK_KERN_PROC_MAX];
    // count of event_index entries
    size_t event_index_next_pos;

    SLIST_HEAD(run_procs_head,     vk_proc)     run_procs;
    SLIST_HEAD(blocked_procs_head, vk_proc) blocked_procs;

    /* fired by signal SIGTERM */
    int shutdown_requested;

    /* signal from async signal handler for synchronous signal handler */
    volatile sig_atomic_t signo;

    union {
        struct vk_fd_table table;
        struct {
            size_t size;
            struct vk_fd fds[VK_FD_MAX];
        } data;
    } fd;
};

struct vk_kern_mainline_udata {
    struct vk_kern *kern_ptr;
    struct vk_proc *proc_ptr;
};

#endif
