#ifndef VK_PROC_LOCAL_S_H
#define VK_PROC_LOCAL_S_H

#include "vk_queue.h"
#include "vk_stack_s.h"

struct vk_thread;

/* dependency encapsulation */
struct vk_proc_local {
	size_t proc_id;

	/* scheduling */
	int run;					 /* proc-write */
	int blocked;					 /* proc-write */
	SLIST_HEAD(run_q_head, vk_thread) run_q;	 /* proc-rw */
	SLIST_HEAD(blocked_q_head, vk_socket) blocked_q; /* proc-rw */

	/* signal handling */
	struct vk_thread* running_cr;	 /* proc-rw */
	struct vk_thread* supervisor_cr; /* proc-rw */
	siginfo_t siginfo;		 /* proc-read */
	ucontext_t* uc_ptr;		 /* proc-read */

	/* memory */
	struct vk_stack stack; /* proc-rw */
};

#endif