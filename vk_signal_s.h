#ifndef VK_SIGNAL_S_H
#define VK_SIGNAL_S_H

#include <signal.h>
#include <setjmp.h>

/* VK_SIGNAL_USE_SIGJMP is more modern, but extra syscall in the hot path, so leave it undefined */
#undef VK_SIGNAL_USE_SIGJMP

struct vk_signal {
    sigset_t current_set;
    sigset_t desired_set;
    void (*handler)(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr);
    void *handler_udata;
    void (*jumper)(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr);
    void *jumper_udata;
    void (*mainline)(void *mainline_udata);
    void *mainline_udata;
#ifdef VK_SIGNAL_USR_SIGJMP
    sigjmp_buf sigenv;
#else
    jmp_buf env;
#endif
    siginfo_t *siginfo_ptr;
    ucontext_t *uc_ptr;
};

#endif