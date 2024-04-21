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
		vk_set_waiting_socket(that, (socket_ptr));                                                             \
		vk_block_set_vk(vk_socket_get_block(socket_ptr), that);                                                \
		vk_yield(VK_PROC_WAIT);                                                                                \
		vk_set_waiting_socket(that, NULL);                                                                     \
	} while (0)

#endif