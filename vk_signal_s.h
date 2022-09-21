#ifndef VK_SIGNAL_S_H
#define VK_SIGNAL_S_H

#include <signal.h>

struct vk_signal {
    sigset_t current_set;
    sigset_t desired_set;
    void (*handler)(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr);
    void *handler_udata;
    void (*jumper)(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr);
    void *jumper_udata;
    sigjmp_buf env;
    siginfo_t *siginfo_ptr;
    ucontext_t *uc_ptr;
};

#endif