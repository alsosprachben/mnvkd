#include "debug.h"

#include "vk_state.h"
#include "vk_proc.h"
#include "vk_proc_s.h"
#include "vk_socket.h"
#include "vk_socket_s.h"

#include "vk_heap.h"

int vk_proc_init(struct vk_proc *proc_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset) {
    int rc;

    rc = vk_heap_map(&proc_ptr->heap, map_addr, map_len, map_prot, map_flags, map_fd, map_offset);
    if (rc == -1) {
        return -1;
    }

    SLIST_INIT(&proc_ptr->run_q);
    SLIST_INIT(&proc_ptr->blocked_q);

    return rc;
}

int vk_proc_deinit(struct vk_proc *proc_ptr) {
    int rc;

    rc = vk_heap_unmap(&proc_ptr->heap);
    if (rc == -1) {
        return -1;
    }

    return rc;
}

struct vk_heap_descriptor *vk_proc_get_heap(struct vk_proc *proc_ptr) {
    return &proc_ptr->heap;
}

void vk_proc_enqueue_run(struct vk_proc *proc_ptr, struct that *that) {
	DBG("NQUEUE@"PRIvk"\n", ARGvk(that));
    if ( ! vk_get_enqueued_run(that)) {
        SLIST_INSERT_HEAD(&proc_ptr->run_q, that, run_q_elem);
        vk_set_enqueued_run(that, 1);
    } else {
        DBG(    "already enqueued.\n");
    }
}

void vk_proc_enqueue_blocked(struct vk_proc *proc_ptr, struct vk_socket *socket_ptr) {
	//DBG("  vk_proc_enqueue_blocked()@"PRIvk"\n", ARGvk(that));
    if ( ! vk_socket_get_enqueued_blocked(socket_ptr)) {
        SLIST_INSERT_HEAD(&proc_ptr->blocked_q, socket_ptr, blocked_q_elem);
        vk_socket_set_enqueued_blocked(socket_ptr, 1);
    } else {
        DBG("    is already enqueued.\n");
    }
}

struct that *vk_proc_dequeue_run(struct vk_proc *proc_ptr) {
    struct that *that;
	DBG("  vk_proc_dequeue_run()\n");

    if (SLIST_EMPTY(&proc_ptr->run_q)) {
        DBG("    is empty.\n");
        return NULL;
    }

    that = SLIST_FIRST(&proc_ptr->run_q);
    SLIST_REMOVE_HEAD(&proc_ptr->run_q, run_q_elem);
    that->run_enq = 0;

	DBG("DQUEUE@"PRIvk"\n", ARGvk(that));

    return that;
}

struct vk_socket *vk_proc_dequeue_blocked(struct vk_proc *proc_ptr) {
    struct vk_socket *socket_ptr;
	DBG("  vk_proc_dequeue_blocked()\n");

    if (SLIST_EMPTY(&proc_ptr->blocked_q)) {
        DBG("    is empty.\n");
        return NULL;
    }

    socket_ptr = SLIST_FIRST(&proc_ptr->blocked_q);
    SLIST_REMOVE_HEAD(&proc_ptr->blocked_q, blocked_q_elem);
    socket_ptr->blocked_enq = 0;
 
 	//DBG("    Dequeued: "PRIvk"\n", ARGvk(that));

    return socket_ptr;
}

int vk_proc_execute(struct vk_proc *proc_ptr) {
	int rc;
    struct that *that;

	rc = vk_heap_enter(vk_proc_get_heap(proc_ptr));
	if (rc == -1) {
		return -1;
	}

    while ( (that = vk_proc_dequeue_run(proc_ptr)) ) {
        DBG("   RUN@"PRIvk"\n", ARGvk(that));
        while (vk_is_ready(that)) {
            DBG("  EXEC@"PRIvk"\n", ARGvk(that));
            that->func(that);
            DBG("  STOP@"PRIvk"\n", ARGvk(that));
            rc = vk_unblock(that);
            if (rc == -1) {
                return -1;
            }
        }

        if (vk_is_yielding(that)) {
            /* 
                * Yielded coroutines are already added to the end of the run queue,
                * but are left in yield state to break out of the preceeding loop,
                * and need to be set back to run state once past the preceeding loop.
                */
            DBG(" YIELD@"PRIvk"\n", ARGvk(that));
            vk_ready(that);
        } 

        SLIST_FOREACH(that, &proc_ptr->run_q, run_q_elem) {
            DBG("   INQ@"PRIvk"\n", ARGvk(that));
        }
	}

	rc = vk_heap_exit(vk_proc_get_heap(proc_ptr));
	if (rc == -1) {
		return -1;
	}

    {
        struct vk_socket *socket_ptr;

        while ( (socket_ptr = vk_proc_dequeue_blocked(proc_ptr)) ) {
        
            if (proc_ptr->nfds < VK_PROC_MAX_EVENTS) {
                io_future_init(&proc_ptr->events[proc_ptr->nfds], socket_ptr);
                proc_ptr->fds[proc_ptr->nfds] = proc_ptr->events[proc_ptr->nfds].event;
                ++proc_ptr->nfds;
            } else {
                DBG("exceeded poll limit");
            }
        }

        DBG("poll(fds, %i, 1000) = ", proc_ptr->nfds);
        rc = poll(proc_ptr->fds, proc_ptr->nfds, 1000);
        DBG("%i\n", rc);
        if (rc == -1) {
            return -1;
        }

        // copy the return events back
        for (int i = 0; i < proc_ptr->nfds; i++) {
            proc_ptr->events[i].event.revents = proc_ptr->fds[i].revents;
            if (proc_ptr->events[i].event.revents) {
                // add to run queue
                rc = vk_socket_handler(proc_ptr->events[i].socket_ptr);
                if (rc == -1) {
                    return -1;
                }
                vk_proc_enqueue_run(proc_ptr, proc_ptr->events[i].socket_ptr->block.blocked_vk);
            } else {
                // back to the poller
                vk_proc_enqueue_blocked(proc_ptr, proc_ptr->events[i].socket_ptr);
            }
        }

        proc_ptr->nfds = 0;
    }

	return 0;
}