
#include "vk_proc_local.h"
#include "vk_proc_local_s.h"
#include "vk_thread_s.h"
#include "vk_socket_s.h"

void vk_proc_local_init(struct vk_proc_local *proc_local_ptr) {
    proc_local_ptr->run = 0;
    proc_local_ptr->blocked = 0;

    proc_local_ptr->nfds = 0;

    SLIST_INIT(&proc_local_ptr->run_q);
    SLIST_INIT(&proc_local_ptr->blocked_q);
}

size_t vk_proc_local_alloc_size() {
    return sizeof (struct vk_proc_local);
}

size_t vk_proc_local_get_proc_id(struct vk_proc_local *proc_local_ptr) {
    return proc_local_ptr->proc_id;
}
void vk_proc_local_set_proc_id(struct vk_proc_local *proc_local_ptr, size_t proc_id) {
    proc_local_ptr->proc_id = proc_id;
}

struct vk_stack *vk_proc_local_get_stack(struct vk_proc_local *proc_local_ptr) {
    return &proc_local_ptr->stack;
}
void vk_proc_local_set_stack(struct vk_proc_local *proc_local_ptr, struct vk_stack *stack_ptr) {
    proc_local_ptr->stack = *stack_ptr;
}

int vk_proc_local_get_run(struct vk_proc_local *proc_local_ptr) {
    return proc_local_ptr->run;
}
void vk_proc_local_set_run(struct vk_proc_local *proc_local_ptr, int run) {
    proc_local_ptr->run = run;
}
int vk_proc_local_get_blocked(struct vk_proc_local *proc_local_ptr) {
    return proc_local_ptr->blocked;
}
void vk_proc_local_set_blocked(struct vk_proc_local *proc_local_ptr, int blocked) {
    proc_local_ptr->blocked = blocked;
}

struct vk_thread *vk_proc_local_first_run(struct vk_proc_local *proc_local_ptr) {
    return SLIST_FIRST(&proc_local_ptr->run_q);
}

struct vk_socket *vk_proc_local_first_blocked(struct vk_proc_local *proc_local_ptr) {
    return SLIST_FIRST(&proc_local_ptr->blocked_q);
}

struct vk_thread *vk_proc_local_get_running(struct vk_proc_local *proc_local_ptr) {
    return proc_local_ptr->running_cr;
}

void vk_proc_local_set_running(struct vk_proc_local *proc_local_ptr, struct vk_thread *that) {
    proc_local_ptr->running_cr = that;
}

struct vk_thread *vk_proc_local_get_supervisor(struct vk_proc_local *proc_local_ptr) {
    return proc_local_ptr->supervisor_cr;
}

void vk_proc_local_set_supervisor(struct vk_proc_local *proc_local_ptr, struct vk_thread *that) {
    proc_local_ptr->supervisor_cr = that;
}

siginfo_t *vk_proc_local_get_siginfo(struct vk_proc_local *proc_local_ptr) {
    return &proc_local_ptr->siginfo;
}

void vk_proc_local_set_siginfo(struct vk_proc_local *proc_local_ptr, siginfo_t siginfo) {
    proc_local_ptr->siginfo = siginfo;
}

ucontext_t *vk_proc_local_get_uc(struct vk_proc_local *proc_local_ptr) {
    return proc_local_ptr->uc_ptr;
}

void vk_proc_local_set_uc(struct vk_proc_local *proc_local_ptr, ucontext_t *uc_ptr) {
    proc_local_ptr->uc_ptr = uc_ptr;
}

void vk_proc_local_clear_signal(struct vk_proc_local *proc_local_ptr) {
    memset(&proc_local_ptr->siginfo, 0, sizeof (proc_local_ptr->siginfo));
    vk_proc_local_set_uc(proc_local_ptr, NULL);
}

int vk_proc_local_is_zombie(struct vk_proc_local *proc_local_ptr) {
    return SLIST_EMPTY(&proc_local_ptr->run_q) && SLIST_EMPTY(&proc_local_ptr->blocked_q);
}

void vk_proc_local_enqueue_run(struct vk_proc_local *proc_local_ptr, struct vk_thread *that) {
    vk_dbg("enqueue thread to run");

    vk_ready(that);
    if (vk_is_ready(that)) {
        if ( ! vk_get_enqueued_run(that)) {
            if (SLIST_EMPTY(&proc_local_ptr->run_q)) {
                proc_local_ptr->run = 1;
	            vk_proc_local_dbg("  which enqueues process to run");
            }

            SLIST_INSERT_HEAD(&proc_local_ptr->run_q, that, run_q_elem);
            vk_set_enqueued_run(that, 1);
        } else {
            vk_dbg("  which is already enqueued to run");
        }
    }
}

void vk_proc_local_enqueue_blocked(struct vk_proc_local *proc_local_ptr, struct vk_socket *socket_ptr) {
    struct vk_thread *that;
    that = socket_ptr->block.blocked_vk;
    vk_dbg("enqueue thread to block");

    if ( ! vk_socket_get_enqueued_blocked(socket_ptr)) {
        if (SLIST_EMPTY(&proc_local_ptr->blocked_q)) {
            proc_local_ptr->blocked = 1;
            vk_proc_local_dbg("  which enqueues process to block");
        }

        SLIST_INSERT_HEAD(&proc_local_ptr->blocked_q, socket_ptr, blocked_q_elem);
        vk_socket_set_enqueued_blocked(socket_ptr, 1);
    } else {
        vk_dbg("  which is already enqueued to block");
    }
}

void vk_proc_local_drop_run(struct vk_proc_local *proc_local_ptr, struct vk_thread *that) {
    vk_dbg("dequeue thread to run");
    if (vk_get_enqueued_run(that)) {
        SLIST_REMOVE(&proc_local_ptr->run_q, that, vk_thread, run_q_elem);
        vk_set_enqueued_run(that, 0);
    }

    if (SLIST_EMPTY(&proc_local_ptr->run_q)) {
        proc_local_ptr->run = 0;
        vk_proc_local_dbg("  which dequeues process to run");
    }
}

void vk_proc_local_drop_blocked_for(struct vk_proc_local *proc_local_ptr, struct vk_thread *that) {
    struct vk_socket *socket_ptr;
    struct vk_socket *socket_cursor_ptr;
    
    SLIST_FOREACH_SAFE(socket_ptr, &proc_local_ptr->blocked_q, blocked_q_elem, socket_cursor_ptr) {
        if (socket_ptr->block.blocked_vk == that) {
            vk_proc_local_drop_blocked(proc_local_ptr, socket_ptr);
        }
    }
}

void vk_proc_local_drop_blocked(struct vk_proc_local *proc_local_ptr, struct vk_socket *socket_ptr) {
    struct vk_thread *that;
    vk_socket_dbg("dequeue socket to unblock");
    that = socket_ptr->block.blocked_vk;
    vk_dbg("  which unblocks thread");

    if (vk_socket_get_enqueued_blocked(socket_ptr)) {
        SLIST_REMOVE(&proc_local_ptr->blocked_q, socket_ptr, vk_socket, blocked_q_elem);
        vk_socket_set_enqueued_blocked(socket_ptr, 0);
    }

    if (SLIST_EMPTY(&proc_local_ptr->blocked_q)) {
        proc_local_ptr->blocked = 0;
        vk_proc_local_dbg("  which unblocks process");
    }
}

struct vk_thread *vk_proc_local_dequeue_run(struct vk_proc_local *proc_local_ptr) {
    struct vk_thread *that;
    vk_proc_local_dbg("getting next runnable process thread");

    if (SLIST_EMPTY(&proc_local_ptr->run_q)) {
        vk_proc_local_dbg("  of which there are none");
        return NULL;
    }

    that = SLIST_FIRST(&proc_local_ptr->run_q);
    SLIST_REMOVE_HEAD(&proc_local_ptr->run_q, run_q_elem);
    vk_set_enqueued_run(that, 0);

    vk_dbg("  which is the next thread");

    if (SLIST_EMPTY(&proc_local_ptr->run_q)) {
        proc_local_ptr->run = 0;
    	vk_proc_local_dbg("  which drains the process of runnable threads");
    }
    return that;
}

struct vk_socket *vk_proc_local_dequeue_blocked(struct vk_proc_local *proc_local_ptr) {
    struct vk_socket *socket_ptr;
    struct vk_thread *that;
    vk_proc_local_dbg("getting next blocked socket");

    if (SLIST_EMPTY(&proc_local_ptr->blocked_q)) {
        vk_proc_local_dbg("  of which there are none");
        return NULL;
    }

    socket_ptr = SLIST_FIRST(&proc_local_ptr->blocked_q);
    SLIST_REMOVE_HEAD(&proc_local_ptr->blocked_q, blocked_q_elem);
    vk_socket_set_enqueued_blocked(socket_ptr, 0);

    vk_socket_dbg("  which is the next socket");

    that = socket_ptr->block.blocked_vk;
    vk_dbg("  which blocked this thread");

    if (SLIST_EMPTY(&proc_local_ptr->blocked_q)) {
        proc_local_ptr->blocked = 0;
        vk_proc_local_dbg("  which drains the process of blocked sockets");
    }

    return socket_ptr;
}

void vk_proc_local_dump_run_q(struct vk_proc_local *proc_local_ptr) {
    struct vk_thread *that;
    SLIST_FOREACH(that, &proc_local_ptr->run_q, run_q_elem) {
	    vk_log("in run queue");
    }
}

void vk_proc_local_dump_blocked_q(struct vk_proc_local *proc_local_ptr) {
    struct vk_socket *socket_ptr;
    SLIST_FOREACH(socket_ptr, &proc_local_ptr->blocked_q, blocked_q_elem) {
	    vk_socket_log("in blocked queue");
    }
}

int vk_proc_local_raise_signal(struct vk_proc_local *proc_local_ptr) {
    struct vk_thread *that;

    that = vk_proc_local_get_running(proc_local_ptr);
    if (that) {
        /* signal handling, so re-enter immediately */
	vk_dbg("SIG");
        if (vk_proc_local_get_supervisor(proc_local_ptr)) {
            /* If a supervisor coroutine is configured for this process, then send the signal to it instead. */
	    that = vk_proc_local_get_supervisor(proc_local_ptr);
	}
	vk_proc_local_enqueue_run(proc_local_ptr, that);
	vk_raise_at(that, EFAULT);
	vk_dbg("RAISED");
        return 1;
    } else {
        return 0;
    }
}

int vk_proc_local_execute(struct vk_proc_local *proc_local_ptr) {
    int rc;
    struct vk_thread *that;

    vk_proc_local_dbg("dispatching process");
    while ( (that = vk_proc_local_dequeue_run(proc_local_ptr)) ) {
        vk_dbg("  which dispatches thread");
        while (vk_is_ready(that)) {
            vk_func func;

            vk_dbg("    calling into thread");
            func = vk_get_func(that);
            vk_proc_local_set_running(proc_local_ptr, that);
            func(that);
            vk_proc_local_set_running(proc_local_ptr, NULL);
            vk_dbg("    returning from thread");

            if (that->status == VK_PROC_END) {
                vk_proc_local_drop_blocked_for(proc_local_ptr, that);
                vk_stack_pop(vk_proc_local_get_stack(proc_local_ptr)); /* self */
                vk_stack_pop(vk_proc_local_get_stack(proc_local_ptr)); /* socket */
                if (vk_get_enqueued_run(that)) {
		            vk_proc_local_drop_run(vk_get_proc_local(that), that);
	            }
            } else {
                /* handle I/O */
                rc = vk_unblock(that);
                if (rc == -1) {
                    return -1;
                }
            }
        }

        if (vk_is_yielding(that)) {
            /* 
                * Yielded coroutines are already added to the end of the run queue,
                * but are left in yield state to break out of the preceding loop,
                * and need to be set back to run state once past the preceding loop.
                */
	        vk_dbg("  yielding from thread");
            vk_ready(that);
        } 

        vk_proc_local_dump_run_q(proc_local_ptr);
	}
    vk_proc_local_dbg("dispatched process");

    return 0;
}
