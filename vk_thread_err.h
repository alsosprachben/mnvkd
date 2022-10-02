#ifndef VK_THREAD_ERR_H
#define VK_THREAD_ERR_H

/*
 * error handling
 */

/* entrypoint for errors */
#define vk_finally() do {           \
	case -2:                        \
		errno = vk_get_error(that); \
} while (0)

/* restart coroutine in ERR state, marking error, continuing at cr_finally() */
#define vk_raise(e) do {                     \
	vk_set_error(that, e);                   \
	vk_set_error_counter(that, vk_get_counter(that)); \
	vk_play(that);                       \
	vk_yield(VK_PROC_ERR);               \
} while (0)

/* restart target coroutine in ERR state, marking error, continuing at cr_finally() */
#define vk_raise_at(there, e) do { \
	vk_set_error(there, e); \
	vk_set_error_counter(there, vk_get_counter(there)); \
	vk_set_status(there, VK_PROC_ERR); \
	vk_play(there); \
} while (0)

/* restart coroutine in ERR state, marking errno as error, continuing at cr_finally() */
#define vk_error() vk_raise(errno)

/* restart target coroutine in ERR state, marking errno as error, continuing at cr_finally() */
#define vk_error_at(there) vk_raise_at(there, errno)

/* restart coroutine in RUN state, clearing the error, continuing back where at cr_raise()/cr_error() */
#define vk_lower() do {                      \
	vk_set_error(that, 0);                     \
	vk_set_counter(that, vk_get_error_counter(that)); \
	vk_play(that);                       \
	vk_yield(VK_PROC_RUN);               \
} while (0)

/* populate buffer with fault signal description for specified thread */
#define vk_snfault_at(there, str, len) { \
	vk_signal_get_siginfo_str(vk_proc_get_siginfo(vk_get_proc(there)), str, len); \
}
/* populate buffer with fault signal description for current thread */
#define vk_snfault(str, len) vk_snfault_at(this, str, len)

#endif