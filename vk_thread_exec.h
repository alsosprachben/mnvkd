#ifndef VK_THREAD_EXEC_H
#define VK_THREAD_EXEC_H

/* schedule the specified coroutine to run */
#define vk_play(there) vk_enqueue_run(there)

/*
 * stop coroutine in YIELD state,
 * which will break out of execution,
 * and immediately be set back to RUN state,
 * ready for vk_play() to add it back to the run queue.
 */
#define vk_pause()                                                                                                     \
	do {                                                                                                           \
		vk_yield(VK_PROC_YIELD);                                                                               \
	} while (0)

/* schedule the callee to run, then stop the coroutine in YIELD state */
#define vk_call(there)                                                                                                 \
	do {                                                                                                           \
		vk_play(there);                                                                                        \
		vk_pause();                                                                                            \
	} while (0)

/* stop coroutine in WAIT state, marking blocked socket */
#define vk_wait(socket_ptr)                                                                                            \
    do {                                                                                                           \
        DBG("vk_wait: that=%p self=%p op=%s\n", (void*)that, vk_get_self(that), vk_block_get_op_str(vk_socket_get_block(socket_ptr))); \
        vk_set_waiting_socket(that, (socket_ptr));                                                             \
        vk_block_set_vk(vk_socket_get_block(socket_ptr), that);                                                \
        vk_yield(VK_PROC_WAIT);                                                                                \
        vk_set_waiting_socket(that, NULL);                                                                     \
    } while (0)

/* stop coroutine in DEFER state, enqueueing it for aggregated I/O */
#define vk_defer(socket_ptr)                                                                                          \
    do {                                                                                                           \
        DBG("vk_defer: that=%p self=%p op=%s\n",                                                                    \
            (void*)that, vk_get_self(that), vk_block_get_op_str(vk_socket_get_block(socket_ptr)));              \
        vk_set_waiting_socket(that, (socket_ptr));                                                             \
        vk_block_set_vk(vk_socket_get_block(socket_ptr), that);                                                \
        vk_proc_local_enqueue_deferred(vk_get_proc_local(that), that);                                         \
        vk_yield(VK_PROC_DEFER);                                                                               \
        vk_set_waiting_socket(that, NULL);                                                                     \
    } while (0)

/* coroutine-scoped for child */
#define vk_child(child, vk_func) VK_INIT_CHILD(that, child, vk_func)

/* coroutine-scoped for responder */
#define vk_responder(child, vk_func) VK_INIT_RESPONDER(that, child, vk_func)

/* coroutine-scoped for accepted socket into new heap */
#define vk_accepted(there, vk_func, rx_fd_arg, tx_fd_arg) VK_INIT(there, that->proc_ptr, vk_func, rx_fd_arg, tx_fd_arg)

#endif
