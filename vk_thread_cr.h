#ifndef VK_THREAD_CR_H
#define VK_THREAD_CR_H

/* state machine macros */

/* continue RUN state, branching to blocked entrypoint, or error entrypoint, and allocate self */
#define vk_begin()                                                                                                     \
    {                                                                                                              \
        self = vk_get_self(that);                                                                              \
        if (vk_get_status(that) == VK_PROC_ERR) {                                                              \
            vk_set_counter(that, -2);                                                                      \
            vk_set_status(that, VK_PROC_RUN);                                                              \
        }                                                                                                      \
        switch (vk_get_counter(that)) {                                                                        \
            case -1:                                                                                       \
                vk_dbg("starting");                                                                    \
                vk_set_file(that, __FILE__);                                                           \
                vk_set_func_name(that, __func__);                                                      \
                vk_calloc(self, 1);                                                                     \
                vk_set_self(that, self)

/* set END state -- any continuations will end up back at the end */
#define vk_end()                                                                                                       \
	default:                                                                                                       \
		vk_set_counter(that, __COUNTER__);                                                                     \
		vk_set_line(that, __LINE__);                                                                           \
		vk_set_status(that, VK_PROC_END);                                                                      \
		vk_dbg("ending");                                                                                      \
	case __COUNTER__ - 1:;                                                                                         \
		}                                                                                                      \
		}                                                                                                      \
		return

/* stop coroutine in specified run state */
#define vk_yield(s)                                                                                                    \
	do {                                                                                                           \
		vk_set_line(that, __LINE__);                                                                           \
		vk_set_counter(that, __COUNTER__);                                                                     \
		vk_set_status(that, s);                                                                                \
		vk_dbg("yielding");                                                                                    \
		return;                                                                                                \
		case __COUNTER__ - 1:;                                                                                 \
			vk_dbg("continue");                                                                            \
	} while (0)

#endif
