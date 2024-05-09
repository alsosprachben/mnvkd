#ifndef VK_THREAD_ERR_H
#define VK_THREAD_ERR_H

#include "vk_signal.h"

/*
 * error handling
 */

/* entrypoint for errors */
#define vk_finally()                                                                                                   \
	do {                                                                                                           \
		case -2:                                                                                               \
			errno = vk_get_error(that);                                                                    \
	} while (0)

/* restart coroutine in ERR state, marking error, continuing at cr_finally() */
#define vk_raise(e) \
	do {                                                                                                           \
		vk_set_line(that, __LINE__);                                                                           \
                vk_set_counter(that, __COUNTER__ + 1);                                                                 \
		vk_play_risen(that, e);                                                                                \
		vk_yield(VK_PROC_ERR);                                                                                 \
	} while (0)

/* restart target coroutine in ERR state, marking error, continuing at cr_finally() */
#define vk_raise_at(there, e)                                                                                          \
	do {                                                                                                           \
		vk_play_risen(there, e);                                                                               \
		vk_set_status(there, VK_PROC_ERR);                                                                     \
	} while (0)

/* restart coroutine in ERR state, marking errno as error, continuing at cr_finally() */
#define vk_error() vk_raise(errno)

/* restart target coroutine in ERR state, marking errno as error, continuing at cr_finally() */
#define vk_error_at(there) vk_raise_at(there, errno)

/* restart coroutine in RUN state, clearing the error, continuing back where at cr_raise()/cr_error() */
/* stop coroutine in specified run state */
#define vk_lower()                                                                                                     \
	do {                                                                                                           \
		vk_unset_error_ctx(that);                                                                              \
		vk_set_status(that, VK_PROC_RUN);                                                                                \
		vk_dbg("lowering");                                                                                    \
		return;                                                                                                \
		case __COUNTER__ - 1:;                                                                                 \
			vk_dbg("continue");                                                                            \
	} while (0)

/* populate buffer with fault signal description for specified thread */
#define vk_snfault_at(there, str, len)                                                                                 \
	vk_signal_get_siginfo_str(vk_proc_local_get_siginfo(vk_get_proc_local(there)), str, len)

/* populate buffer with fault signal description for current thread */
#define vk_snfault(str, len) vk_snfault_at(that, str, len)

#define vk_get_signal_at(there) (vk_proc_local_get_siginfo(vk_get_proc_local(there))->si_signo)
#define vk_get_signal() vk_get_signal_at(that)
#define vk_clear_signal() vk_proc_local_clear_signal(vk_get_proc_local(that))

#define vk_sigerror()                                                                                                  \
do {                                                                                                                   \
	char __vk_sigerror_line[256];                                                                                  \
	if (errno == EFAULT && vk_get_signal() != 0) {                                                                 \
		/* interrupted by signal */                                                                            \
		rc = vk_snfault(__vk_sigerror_line, sizeof(__vk_sigerror_line) - 1);                                   \
		if (rc == -1) {                                                                                        \
			vk_perror("vk_snfault");                                                                       \
		} else {                                                                                               \
			vk_perror(__vk_sigerror_line);                                                                 \
		}                                                                                                      \
		vk_clear_signal();                                                                                     \
	}                                                                                                              \
} while (0)

#endif