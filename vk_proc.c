/* Copyright 2022 BCW. All Rights Reserved. */
#include <string.h>

#include "vk_debug.h"

#include "vk_thread.h"
#include "vk_thread_s.h"
#include "vk_proc.h"
#include "vk_proc_s.h"
#include "vk_proc_local.h"
#include "vk_proc_local_s.h"
#include "vk_socket.h"
#include "vk_socket_s.h"

#include "vk_heap.h"
#include "vk_kern.h"
#include "vk_pool.h"

void vk_proc_clear(struct vk_proc *proc_ptr) {
    size_t proc_id;

    proc_id = proc_ptr->proc_id;
    memset(proc_ptr, 0, sizeof (*proc_ptr));
    proc_ptr->proc_id = proc_id;
}

struct vk_proc_local *vk_proc_get_local(struct vk_proc *proc_ptr) {
    return proc_ptr->local_ptr;
}

void vk_proc_init(struct vk_proc *proc_ptr) {
    proc_ptr->run_qed = 0;
    proc_ptr->blocked_qed = 0;

    proc_ptr->nfds = 0;

    vk_proc_local_init(proc_ptr->local_ptr);
}

int vk_proc_alloc(struct vk_proc *proc_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset, int entered) {
    int rc;
    struct vk_heap heap;
    memset(&heap, 0, sizeof (heap));

    rc = vk_heap_map(&heap, map_addr, map_len, map_prot, map_flags, map_fd, map_offset, entered);
    if (rc == -1) {
        return -1;
    }

    if (!entered) {
        rc = vk_heap_enter(&heap);
        if (rc == -1) {
            return -1;
        }
    }
    proc_ptr->local_ptr = vk_stack_push(vk_heap_get_stack(&heap), 1, vk_proc_local_alloc_size());
    if (proc_ptr->local_ptr == NULL) {
        int errno2;
        errno2 = errno;
        vk_heap_unmap(&heap);
        errno = errno2;
        return -1;
    }

    vk_proc_local_set_proc_id(vk_proc_get_local(proc_ptr), proc_ptr->proc_id);
    vk_proc_local_set_stack(vk_proc_get_local(proc_ptr), &heap.stack);

    vk_proc_init(proc_ptr);

    if (!entered) {
        rc = vk_heap_exit(&heap);
        if (rc == -1) {
            return -1;
        }
    }

    proc_ptr->heap = heap;

    return 0;
}

int vk_proc_alloc_from_pool(struct vk_proc *proc_ptr, struct vk_pool *pool_ptr) {
    struct vk_pool_entry *entry_ptr;

    entry_ptr = vk_pool_alloc_entry(pool_ptr);
    if (entry_ptr == NULL) {
        return -1;
    }

    proc_ptr->heap = *vk_pool_entry_get_heap(entry_ptr);
    vk_heap_enter(vk_proc_get_heap(proc_ptr));

    proc_ptr->local_ptr = vk_stack_push(vk_heap_get_stack(vk_proc_get_heap(proc_ptr)), 1, vk_proc_local_alloc_size());
    if (proc_ptr->local_ptr == NULL) {
        int errno2;
        errno2 = errno;
        vk_pool_free_entry(pool_ptr, entry_ptr);
        errno = errno2;
        return -1;
    }

    vk_proc_local_set_proc_id(vk_proc_get_local(proc_ptr), proc_ptr->proc_id);
    vk_proc_local_set_stack(vk_proc_get_local(proc_ptr), vk_heap_get_stack(vk_proc_get_heap(proc_ptr)));

    proc_ptr->pool_entry_id = vk_pool_entry_get_id(entry_ptr);

    vk_proc_init(proc_ptr);

    return 0;
}

int vk_proc_free(struct vk_proc *proc_ptr) {
    int rc;

    rc = vk_heap_unmap(vk_proc_get_heap(proc_ptr));
    if (rc == -1) {
        return -1;
    }

    return rc;
}

int vk_proc_free_from_pool(struct vk_proc *proc_ptr, struct vk_pool *pool_ptr) {
    struct vk_pool_entry *entry_ptr;

    entry_ptr = vk_pool_get_entry(pool_ptr, proc_ptr->pool_entry_id);
    if (entry_ptr == NULL) {
        return -1;
    }

    return vk_pool_free_entry(pool_ptr, entry_ptr);
}

struct vk_thread *vk_proc_alloc_thread(struct vk_proc *proc_ptr) {
    int rc;
    struct vk_thread *that;
    
    rc = vk_heap_enter(vk_proc_get_heap(proc_ptr));
    if (rc == -1) {
        return NULL;
    }

    that = vk_stack_push(vk_proc_local_get_stack(proc_ptr->local_ptr), sizeof (*that), 1);
    if (that == NULL) {
        return NULL;
    }

    /* leave heap open */

    return that;
}

int vk_proc_free_thread(struct vk_proc *proc_ptr) {
    return vk_stack_pop(vk_proc_local_get_stack(proc_ptr->local_ptr));
}

size_t vk_proc_alloc_size() {
    return sizeof (struct vk_proc);
}

struct vk_heap *vk_proc_get_heap(struct vk_proc *proc_ptr) {
    return &proc_ptr->heap;
}

struct vk_proc *vk_proc_next_run_proc(struct vk_proc *proc_ptr) {
    return SLIST_NEXT(proc_ptr, run_list_elem);
}

struct vk_proc *vk_proc_next_blocked_proc(struct vk_proc *proc_ptr) {
    return SLIST_NEXT(proc_ptr, blocked_list_elem);
}

int vk_proc_prepoll(struct vk_proc *proc_ptr) {
    struct vk_socket *socket_ptr;
    struct vk_proc_local *proc_local_ptr;
    proc_local_ptr = vk_proc_get_local(proc_ptr);

    proc_ptr->nfds = 0;
    proc_local_ptr->nfds = 0;
    socket_ptr = vk_proc_local_first_blocked(proc_local_ptr);
    while (socket_ptr) {
        if (proc_local_ptr->nfds < VK_PROC_MAX_EVENTS) {
            io_future_init(&proc_local_ptr->events[proc_local_ptr->nfds], socket_ptr);
            DBG("Process(%zu)[%i] = %i: %i\n", proc_ptr->proc_id, proc_local_ptr->nfds, proc_local_ptr->events[proc_local_ptr->nfds].event.fd, (int) (proc_local_ptr->events[proc_local_ptr->nfds].event.events));
            proc_ptr->fds[proc_ptr->nfds] = proc_local_ptr->events[proc_local_ptr->nfds].event;
            ++proc_local_ptr->nfds;
            ++proc_ptr->nfds;
        } else {
            DBG("exceeded poll limit");
            errno = ENOBUFS;
            return -1;
        }
        socket_ptr = vk_socket_next_blocked_socket(socket_ptr);
    }

    return 0;
}

int vk_proc_postpoll(struct vk_proc *proc_ptr) {
    int rc;
    struct vk_proc_local *proc_local_ptr;
    proc_local_ptr = vk_proc_get_local(proc_ptr);

    // copy the return events back
    for (int i = 0; i < proc_local_ptr->nfds; i++) {
        proc_local_ptr->events[i].event.revents = proc_ptr->fds[i].revents;
        if (proc_local_ptr->events[i].event.revents) {
            // add to run queue
            rc = vk_socket_handler(proc_local_ptr->events[i].socket_ptr);
            if (rc == -1) {
                return -1;
            }

            vk_proc_local_drop_blocked(proc_local_ptr, proc_local_ptr->events[i].socket_ptr);
            vk_proc_local_enqueue_run(proc_local_ptr, proc_local_ptr->events[i].socket_ptr->block.blocked_vk);
        } else {
            // back to the poller
            vk_proc_local_enqueue_blocked(proc_local_ptr, proc_local_ptr->events[i].socket_ptr);
        }
    }

    proc_local_ptr->nfds = 0;

	return 0;
}

int vk_proc_execute(struct vk_proc *proc_ptr) {
	int rc;
    struct vk_proc_local *proc_local_ptr;
    proc_local_ptr = vk_proc_get_local(proc_ptr);

    rc = vk_proc_local_raise_signal(proc_local_ptr);
    if (! rc) {
        rc = vk_proc_postpoll(proc_ptr);
        if (rc == -1) {
            return -1;
        }
    }

    rc = vk_proc_local_execute(proc_local_ptr);
    if (rc == -1) {
        return -1;
    }

    rc = vk_proc_prepoll(proc_ptr);
    if (rc == -1) {
        return -1;
    }

    return 0;
}

int vk_proc_poll(struct vk_proc *proc_ptr) {
    int rc;

    DBG("poll(fds, %i, 1000) = ", proc_ptr->nfds);
    do {
        rc = poll(proc_ptr->fds, proc_ptr->nfds, 1000);
    } while (rc == 0);
    DBG("%i\n", rc);
    if (rc == -1) {
        return -1;
    }

    return 0;
}
