/* Copyright 2022 BCW. All Rights Reserved. */
#include <string.h>
#include <poll.h>
#include <stdlib.h>

#include "vk_kern.h"
#include "vk_kern_s.h"

#include "vk_signal.h"

void vk_kern_clear(struct vk_kern *kern_ptr) {
    memset(kern_ptr, 0, sizeof (*kern_ptr));
}

void vk_kern_set_shutdown_requested(struct vk_kern *kern_ptr) {
    kern_ptr->shutdown_requested = 1;
}
void vk_kern_clear_shutdown_requested(struct vk_kern *kern_ptr) {
    kern_ptr->shutdown_requested = 1;
}
int vk_kern_get_shutdown_requested(struct vk_kern *kern_ptr) {
    return kern_ptr->shutdown_requested;
}

void vk_kern_signal_handler(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr) {
    struct vk_kern *kern_ptr;
    kern_ptr = (struct vk_kern *) handler_udata;
    int rc;
    char buf[256];

    if (jump == 0) {
        /* system-level signals */
        switch (siginfo_ptr->si_signo) {
            case SIGINT:
            case SIGQUIT:
                /* hard exit */
                rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
                if (rc == -1) {
                    return;
                }
                buf[rc - 1] = '\n';
                fprintf(stderr, "Immediate exit due to signal %i: %s\n", siginfo_ptr->si_signo, buf);
                exit(0);
                break;
            case SIGTERM:
                /* soft exit */
                rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
                if (rc == -1) {
                    return;
                }
                buf[rc - 1] = '\n';
                fprintf(stderr, "Exit request received via signal %i: %s\n", siginfo_ptr->si_signo, buf);
                vk_kern_set_shutdown_requested(kern_ptr);
                break;
        }
    }
}

void vk_kern_signal_jumper(void *handler_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr) {
    struct vk_kern *kern_ptr;
    kern_ptr = (struct vk_kern *) handler_udata;
    int rc;
    char buf[256];

    /* system-level signals */

    rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
    if (rc == -1) {
        return;
    }
    buf[rc - 1] = '\n';
    fprintf(stderr, "Panic due to signal %i: %s\n", siginfo_ptr->si_signo, buf);
    exit(1);
}

struct vk_kern *vk_kern_alloc(struct vk_heap *hd_ptr) {
    struct vk_kern *kern_ptr;
    int rc;
    int i;

	rc = vk_heap_map(hd_ptr, NULL, 4096 * VK_KERN_PAGE_COUNT, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if (rc == -1) {
		return NULL;
	}

	kern_ptr = vk_heap_push(hd_ptr, vk_kern_alloc_size(), 1);
	if (kern_ptr == NULL) {
		return NULL;
	}

    kern_ptr->hd_ptr = hd_ptr;

    SLIST_INIT(&kern_ptr->free_procs);

    // build free-list from top-down
    for (i = VK_KERN_PROC_MAX - 1; i >= 0; i--) {
        SLIST_INSERT_HEAD(&kern_ptr->free_procs, &kern_ptr->proc_table[i], free_list_elem);
        kern_ptr->proc_table[i].proc_id = i;
        kern_ptr->proc_table[i].kern_ptr = kern_ptr;
    }

    kern_ptr->proc_count = 0;
    kern_ptr->nfds = 0;

    kern_ptr->event_index_next_pos = 0;
    kern_ptr->event_proc_next_pos = 0;

    rc = vk_signal_init();
    if (rc == -1) {
        return NULL;
    }
    vk_signal_set_handler(vk_kern_signal_handler, (void *) kern_ptr);
    vk_signal_set_jumper(vk_kern_signal_jumper, (void *) kern_ptr);

    return kern_ptr;
}

size_t vk_kern_alloc_size() {
    return sizeof (struct vk_kern);
}

struct vk_proc *vk_kern_alloc_proc(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    if (SLIST_EMPTY(&kern_ptr->free_procs)) {
        errno = ENOMEM;
        return NULL;
    }

    proc_ptr = SLIST_FIRST(&kern_ptr->free_procs);
    SLIST_REMOVE_HEAD(&kern_ptr->free_procs, free_list_elem);

    DBG("Allocated Process ID %zu\n", proc_ptr->proc_id);

    return proc_ptr;
}

void vk_kern_free_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    SLIST_INSERT_HEAD(&kern_ptr->free_procs, proc_ptr, free_list_elem);
}

struct vk_proc *vk_kern_first_run(struct vk_kern *kern_ptr) {
    return SLIST_FIRST(&kern_ptr->run_procs);
}

struct vk_proc *vk_kern_first_blocked(struct vk_kern *kern_ptr) {
    return SLIST_FIRST(&kern_ptr->blocked_procs);
}

void vk_kern_enqueue_run(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    if ( ! proc_ptr->run_qed) {
        proc_ptr->run_qed = 1;
        DBG("NQUEUE@%zu\n", proc_ptr->proc_id);
        SLIST_INSERT_HEAD(&kern_ptr->run_procs, proc_ptr, run_list_elem);
    }
}

void vk_kern_enqueue_blocked(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    if ( ! proc_ptr->blocked_qed) {
        proc_ptr->blocked_qed = 1;
        DBG("NBLOCK@%zu\n", proc_ptr->proc_id);
        SLIST_INSERT_HEAD(&kern_ptr->blocked_procs, proc_ptr, blocked_list_elem);
    }
}

void vk_kern_drop_run(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    if (proc_ptr->run_qed) {
        proc_ptr->run_qed = 0;
        DBG("DQUEUE@%zu\n", proc_ptr->proc_id);
        SLIST_REMOVE(&kern_ptr->run_procs, proc_ptr, vk_proc, run_list_elem);
    }
}

void vk_kern_drop_blocked(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    if (proc_ptr->blocked_qed) {
        proc_ptr->blocked_qed = 0;
        DBG("DBLOCK@%zu\n", proc_ptr->proc_id);
        SLIST_REMOVE(&kern_ptr->blocked_procs, proc_ptr, vk_proc, blocked_list_elem);
    }
}

struct vk_proc *vk_kern_dequeue_run(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    if (SLIST_EMPTY(&kern_ptr->run_procs)) {
        return NULL;
    }

    proc_ptr = SLIST_FIRST(&kern_ptr->run_procs);
    SLIST_REMOVE_HEAD(&kern_ptr->run_procs, run_list_elem);
    proc_ptr->run_qed = 0;
    DBG("DQUEUE@%zu\n", proc_ptr->proc_id);

    return proc_ptr;
}

struct vk_proc *vk_kern_dequeue_blocked(struct vk_kern *kern_ptr) {
    struct vk_proc *proc_ptr;

    if (SLIST_EMPTY(&kern_ptr->blocked_procs)) {
        return NULL;
    }

    proc_ptr = SLIST_FIRST(&kern_ptr->blocked_procs);
    SLIST_REMOVE_HEAD(&kern_ptr->blocked_procs, blocked_list_elem);
    proc_ptr->blocked_qed = 0;
    DBG("DBLOCK@%zu\n", proc_ptr->proc_id);

    return proc_ptr;
}

void vk_kern_flush_proc_queues(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    if (proc_ptr->run) {
        vk_kern_enqueue_run(kern_ptr, proc_ptr);
    } else {
        vk_kern_drop_run(kern_ptr, proc_ptr);
    }

    if (proc_ptr->blocked) {
        vk_kern_enqueue_blocked(kern_ptr, proc_ptr);
    } else {
        vk_kern_drop_blocked(kern_ptr, proc_ptr);
    }
}

int vk_kern_pending(struct vk_kern *kern_ptr) {
    return ! (SLIST_EMPTY(&kern_ptr->run_procs) && SLIST_EMPTY(&kern_ptr->blocked_procs));
}

/* copy input poll events from procs to kernel */
int vk_kern_prepoll_proc(struct vk_kern *kern_ptr, struct vk_proc *proc_ptr) {
    struct vk_kern_event_index *event_index_ptr;

    if (proc_ptr->nfds == 0) {
        /* Skip adding poll events if none needed. */
        return 0;
    }

    /* copy the blocks to the system */
    event_index_ptr = &kern_ptr->event_index[kern_ptr->event_index_next_pos];
    event_index_ptr->proc_id = proc_ptr->proc_id;
    event_index_ptr->event_start_pos = kern_ptr->event_proc_next_pos;
    event_index_ptr->nfds = proc_ptr->nfds;
    memcpy(&kern_ptr->events[event_index_ptr->event_start_pos], &proc_ptr->fds[0], sizeof (struct pollfd) * proc_ptr->nfds);
    kern_ptr->nfds += proc_ptr->nfds;
    kern_ptr->event_proc_next_pos += proc_ptr->nfds;
    kern_ptr->event_index_next_pos++;

    return 0;
}

int vk_kern_prepoll(struct vk_kern *kern_ptr) {
    int rc;
    struct vk_proc *proc_ptr;

    /* build event index, copying input poll events from procs to kernel */
    kern_ptr->nfds = 0;
    kern_ptr->event_proc_next_pos = 0;
    kern_ptr->event_index_next_pos = 0;
    proc_ptr = vk_kern_first_blocked(kern_ptr);
    while (proc_ptr) {
        rc = vk_kern_prepoll_proc(kern_ptr, proc_ptr);
        if (rc == -1) {
            return -1;
        }
        proc_ptr = vk_proc_next_blocked_proc(proc_ptr);
    }

    return 0;
}

int vk_kern_postpoll(struct vk_kern *kern_ptr) {
    int rc;
    size_t i;
    struct vk_proc *proc_ptr;
    struct vk_kern_event_index *event_index_ptr;

    /* traverse event index, copying output poll events, from kernel to procs */
    for (i = 0; i < kern_ptr->event_index_next_pos; i++) {
        event_index_ptr = &kern_ptr->event_index[i];
        proc_ptr = &kern_ptr->proc_table[event_index_ptr->proc_id];
        memcpy(&proc_ptr->fds[0], &kern_ptr->events[event_index_ptr->event_start_pos], sizeof (struct pollfd) * event_index_ptr->nfds);
    
        rc = vk_proc_execute(proc_ptr);
        if (rc == -1) {
            return -1;
        }
    }

    /* dispatch new runnable procs */
    while ( (proc_ptr = vk_kern_dequeue_run(kern_ptr)) ) {
        proc_ptr->run_qed = 0;
        rc = vk_proc_execute(proc_ptr);
        if (rc == -1) {
            return -1;
        }
    }

    return 0;
}

int vk_kern_poll(struct vk_kern *kern_ptr) {
    int rc;
    size_t i;
    size_t j;

    struct vk_kern_event_index *index_ptr;
    struct pollfd *fd_ptr;

    for (i = 0; i < kern_ptr->event_index_next_pos; i++) {
        index_ptr = &kern_ptr->event_index[i];

        for (j = 0; j < index_ptr->nfds; j++) {
            fd_ptr = &kern_ptr->events[index_ptr->event_start_pos + j];

            DBG("Polling Process ID %zu for FD %i, event mask %i.\n", index_ptr->proc_id, fd_ptr->fd, (int) fd_ptr->events);
        }
    }

    do {
        DBG("poll(..., %i, 1000)", kern_ptr->nfds);
        rc = poll(kern_ptr->events, kern_ptr->nfds, 1000);
        DBG(" = %i\n", rc);
    } while (rc == 0 || (rc == -1 && errno == EINTR));
    if (rc == -1) {
        return -1;
    }

    return 0;
}

int vk_kern_loop(struct vk_kern *kern_ptr) {
    int rc;

	rc = vk_kern_postpoll(kern_ptr);
	if (rc == -1) {
		return -1;
	}
	do {

		rc = vk_kern_prepoll(kern_ptr);
		if (rc == -1) {
			return -1;
		}

		rc = vk_kern_poll(kern_ptr);
		if (rc == -1) {
			return -1;
		}

		rc = vk_kern_postpoll(kern_ptr);
		if (rc == -1) {
			return -1;
		}
	} while (vk_kern_pending(kern_ptr));

    return 0;
}