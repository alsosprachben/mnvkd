#include "vk_signal.h"
#include "vk_signal_s.h"

#include <string.h>
#include <setjmp.h>

struct vk_signal global_vk_signal;

/* set the signal mask to the previously taken memo */
int vk_signal_restore(struct vk_signal *signal_ptr) {
    signal_ptr->current_set = signal_ptr->desired_set;
    return sigprocmask(SIG_SETMASK, &signal_ptr->desired_set, NULL);
}

/* take a memo of the current signal mask */
int vk_signal_save(struct vk_signal *signal_ptr) {
    int rc;

    rc = sigprocmask(SIG_SETMASK, NULL, &signal_ptr->current_set);
    if (rc == -1) {
        return -1;
    }

    signal_ptr->desired_set = signal_ptr->current_set;

    return 0;
}

void vk_signal_block(struct vk_signal *signal_ptr, int signal) {
    (void) sigaddset(&signal_ptr->desired_set, signal);
}

void vk_signal_unblock(struct vk_signal *signal_ptr, int signal) {
    (void) sigdelset(&signal_ptr->desired_set, signal);
}

void vk_signal_setjmp() {
    int rc;

#ifdef VK_SIGNAL_USE_SIGJMP
    rc = sigsetjmp(global_vk_signal.sigenv, 1);
#else
    rc = setjmp(global_vk_signal.env);
#endif
    if (rc == 1) {
        if (global_vk_signal.jumper != NULL) {
            global_vk_signal.jumper(global_vk_signal.jumper_udata, global_vk_signal.siginfo_ptr, global_vk_signal.uc_ptr);
        }
    }
    printf("returning from setjmp\n");
}

void vk_signal_set_handler(void (*handler)(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *handler_udata) {
    global_vk_signal.handler = handler;
    global_vk_signal.handler_udata = handler_udata;
}

void vk_signal_set_jumper(void (*jumper)(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *jumper_udata) {
    global_vk_signal.jumper = jumper;
    global_vk_signal.jumper_udata = jumper_udata;
}

void vk_signal_handler(struct vk_signal *signal_ptr, int signal, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr) {
    int jump;
#ifndef VK_SIGNAL_USE_SIGJMP /* NOT DEFINED */
    int rc;

    rc = vk_signal_restore(signal_ptr);
    if (rc == -1) {
        /* panic */
        return;
    }
#endif

    switch (signal) {
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
#ifdef SIGINFO
        case SIGINFO:
#endif
            /* user signals */
            jump = 0;
            break;
        case SIGABRT:
        case SIGFPE:
        case SIGBUS:
        case SIGSEGV:
        case SIGSYS:
            /* software errors */
            jump = 1; /* Cannot return to code, so jump to callback */
            break;
    }
    if (signal_ptr->handler != NULL) {
        signal_ptr->handler(signal_ptr->handler_udata, jump, siginfo_ptr, uc_ptr);
    }

    if (jump) {
        printf("prepping jump\n");
        signal_ptr->siginfo_ptr = siginfo_ptr;
        signal_ptr->uc_ptr = uc_ptr;
#ifdef VK_SIGNAL_USE_SIGJMP
        siglongjmp(signal_ptr->sigenv, 1);
#else
        printf("long jumping\n");
        longjmp(signal_ptr->env, 1);
#endif
    }
}

void vk_signal_action(int signal, siginfo_t *siginfo_ptr, void *handler_udata) {
    vk_signal_handler(&global_vk_signal, signal, siginfo_ptr, (ucontext_t *) handler_udata);
}

int vk_signal_register(struct vk_signal *signal_ptr) {
    int rc;
    struct sigaction action;
    struct sigaction ignore;

    memset(&action, 0, sizeof (struct sigaction));
    action.sa_sigaction = vk_signal_action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO|SA_NODEFER|SA_RESTART;

    memset(&ignore, 0, sizeof (struct sigaction));
    ignore.sa_sigaction = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
 
    /* user signals */
    rc = sigaction(SIGHUP, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGINT, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGQUIT, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGTERM, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGUSR1, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGUSR2, &action, 0);
    if (rc == -1) {
        return -1;
    }

#ifdef SIGINFO
    rc = sigaction(SIGINFO, &action, 0);
    if (rc == -1) {
        return -1;
    }
#endif

    /* software errors */
    rc = sigaction(SIGABRT, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGFPE, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGBUS, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGSEGV, &action, 0);
    if (rc == -1) {
        return -1;
    }

    rc = sigaction(SIGSYS, &action, 0);
    if (rc == -1) {
        return -1;
    }

    /* design logic */
    rc = sigaction(SIGPIPE, &ignore, 0);
    if (rc == -1) {
        return -1;
    }

    rc = vk_signal_restore(signal_ptr);
    if (rc == -1) {
        return -1;
    }

#ifndef VK_SIGNAL_USE_SIGJMP /* NOT DEFINED */
    rc = vk_signal_save(signal_ptr);
    if (rc == -1) {
        return -1;
    }
#endif

    return 0;
}

int vk_signal_init() {
    memset(&global_vk_signal, 0, sizeof (global_vk_signal));
    return vk_signal_register(&global_vk_signal);
}

#ifdef VK_SIGNAL_TEST
#include <stdlib.h>
#include <stdio.h>

void logic() {
    int rc;
    printf("division by zero\n");
    rc = 5 / 0;
    printf("null access\n");
    rc = *((int *) NULL);
}

void test_jumper(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr) {
    int c;
    c = *((int *) jumper_udata);

    printf("Recovering from signal %i in cycle %i\n", siginfo_ptr->si_signo, c);

    if (++c >= 10) {
        exit(0);
    }
    *((int *) jumper_udata) = c;

    logic();
}
void test_handler(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr) {
    printf("Caught signal %i with jump %i\n", siginfo_ptr->si_signo, jump);
}

int main() {
    int rc;
    int c;
    c = 0;
    rc = vk_signal_init();
    if (rc == -1) {
        return 1;
    }
    vk_signal_set_handler(test_handler, NULL);
    vk_signal_set_jumper(test_jumper, &c);
    vk_signal_setjmp();

    logic();

    return 0;
}
#endif