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
#include "vk_fd.h"
#include "vk_fd_table.h"

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

    vk_proc_local_init(proc_ptr->local_ptr);
}

size_t vk_proc_get_id(struct vk_proc *proc_ptr) {
    return proc_ptr->proc_id;
}
void vk_proc_set_id(struct vk_proc *proc_ptr, size_t proc_id) {
    proc_ptr->proc_id = proc_id;
}

size_t vk_proc_get_pool_entry_id(struct vk_proc *proc_ptr) {
	return proc_ptr->pool_entry_id;
}
void vk_proc_set_pool_entry_id(struct vk_proc *proc_ptr, size_t pool_entry_id) {
	proc_ptr->pool_entry_id = pool_entry_id;
}
struct vk_pool *vk_proc_get_pool(struct vk_proc *proc_ptr) {
	return proc_ptr->pool_ptr;
}
void vk_proc_set_pool(struct vk_proc *proc_ptr, struct vk_pool *pool_ptr) {
	proc_ptr->pool_ptr = pool_ptr;
}
struct vk_pool_entry *vk_proc_get_entry(struct vk_proc *proc_ptr) {
	return proc_ptr->entry_ptr;
}
void vk_proc_set_entry(struct vk_proc *proc_ptr, struct vk_pool_entry *entry_ptr) {
	proc_ptr->entry_ptr = entry_ptr;
}

int vk_proc_get_run(struct vk_proc *proc_ptr) {
    return proc_ptr->run_qed;
}
void vk_proc_set_run(struct vk_proc *proc_ptr, int run) {
    proc_ptr->run_qed = run;
}

int vk_proc_get_blocked(struct vk_proc *proc_ptr) {
    return proc_ptr->blocked_qed;
}
void vk_proc_set_blocked(struct vk_proc *proc_ptr, int blocked) {
    proc_ptr->blocked_qed = blocked;
}

int vk_proc_is_zombie(struct vk_proc *proc_ptr) {
    return ! (proc_ptr->run_qed || proc_ptr->blocked_qed);
}

struct vk_fd *vk_proc_first_fd(struct vk_proc *proc_ptr) {
    return SLIST_FIRST(&proc_ptr->allocated_fds);
}

void vk_proc_allocate_fd(struct vk_proc *proc_ptr, struct vk_fd *fd_ptr, int fd) {
    vk_proc_dbgf("allocating FD %i to process\n", fd);
    if ( ! vk_fd_get_allocated(fd_ptr)) {
        vk_fd_allocate(fd_ptr, fd, proc_ptr->proc_id);
        SLIST_INSERT_HEAD(&proc_ptr->allocated_fds, fd_ptr, allocated_list_elem);
        vk_fd_dbg("allocated");
    }
}

void vk_proc_deallocate_fd(struct vk_proc *proc_ptr, struct vk_fd *fd_ptr) {
    vk_fd_dbgf("deallocating from process %zu\n", proc_ptr->proc_id);
    if (vk_fd_get_allocated(fd_ptr)) {
        vk_fd_set_allocated(fd_ptr, 0);
        SLIST_REMOVE(&proc_ptr->allocated_fds, fd_ptr, vk_fd, allocated_list_elem);
        vk_proc_dbg("deallocated");
    }
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
    proc_ptr->pool_ptr = pool_ptr;
    proc_ptr->entry_ptr = entry_ptr;

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

    that = vk_stack_push(vk_proc_local_get_stack(proc_ptr->local_ptr), 1, sizeof (*that));
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

int vk_proc_prepoll(struct vk_proc *proc_ptr, struct vk_fd_table *fd_table_ptr) {
    struct vk_socket *socket_ptr;
    struct vk_fd *cursor_fd_ptr;
    struct vk_fd *fd_ptr;
    struct vk_proc_local *proc_local_ptr;

    vk_proc_dbg("prepoll");

    cursor_fd_ptr = vk_proc_first_fd(proc_ptr);
    while (cursor_fd_ptr) {
        fd_ptr = cursor_fd_ptr;
        vk_fd_table_prepoll_fd(fd_table_ptr, fd_ptr);

        cursor_fd_ptr = vk_fd_next_allocated_fd(cursor_fd_ptr);
        if (fd_ptr->closed) {
            vk_proc_deallocate_fd(proc_ptr, fd_ptr);
        }
    }

    proc_local_ptr = vk_proc_get_local(proc_ptr);

    socket_ptr = vk_proc_local_first_blocked(proc_local_ptr);
    while (socket_ptr) {
        vk_fd_table_prepoll_blocked_socket(fd_table_ptr, socket_ptr, proc_ptr);

        socket_ptr = vk_socket_next_blocked_socket(socket_ptr);
    }

    return 0;
}

int vk_proc_postpoll(struct vk_proc *proc_ptr, struct vk_fd_table *fd_table_ptr) {
    struct vk_socket *socket_ptr;
    struct vk_fd *fd_ptr;
    struct vk_proc_local *proc_local_ptr;
    int rc;
    int fd;

    vk_proc_dbg("prepoll");

    proc_local_ptr = vk_proc_get_local(proc_ptr);

    socket_ptr = vk_proc_local_first_blocked(proc_local_ptr);
    while (socket_ptr) {
        fd = vk_socket_get_blocked_fd(socket_ptr);
        if (fd == -1) {
            vk_socket_dbg("Socket is not blocked on an FD, so nothing to poll for it.");
            return 0;
        }

        fd_ptr = vk_fd_table_get(fd_table_ptr, fd);
        if (fd_ptr == NULL) {
            return -1;
        }

        rc = vk_fd_table_postpoll_fd(fd_table_ptr, fd_ptr);
        if (rc) {
            rc = vk_proc_local_retry_socket(proc_local_ptr, socket_ptr);
            if (rc == -1) {
                return -1;
            }
        }

        socket_ptr = vk_socket_next_blocked_socket(socket_ptr);
    }

    return 0;
}

int vk_proc_execute(struct vk_proc *proc_ptr, struct vk_fd_table *fd_table_ptr) {
	int rc;
    struct vk_proc_local *proc_local_ptr;
    proc_local_ptr = vk_proc_get_local(proc_ptr);

    rc = vk_proc_local_raise_signal(proc_local_ptr);
    if (! rc) {
        rc = vk_proc_postpoll(proc_ptr, fd_table_ptr);
        if (rc == -1) {
            return -1;
        }
    }

    rc = vk_proc_local_execute(proc_local_ptr);
    if (rc == -1) {
        return -1;
    }

    rc = vk_proc_prepoll(proc_ptr, fd_table_ptr);
    if (rc == -1) {
        return -1;
    }

    return 0;
}