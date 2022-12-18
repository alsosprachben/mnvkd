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

struct vk_kern {
    struct vk_heap *hd_ptr;

    // file descriptors
    struct vk_fd_table *fd_table_ptr;

    // processes
    struct vk_pool_entry *entry_table;
    struct vk_pool proc_pool;
    struct vk_proc *proc_table;

    SLIST_HEAD(run_procs_head,     vk_proc)     run_procs;
    SLIST_HEAD(blocked_procs_head, vk_proc) blocked_procs;

    /* fired by signal SIGTERM */
    int shutdown_requested;

    /* signal from async signal handler for synchronous signal handler */
    volatile sig_atomic_t signo;
};

struct vk_kern_mainline_udata {
    struct vk_kern *kern_ptr;
    struct vk_proc *proc_ptr;
};

#endif
