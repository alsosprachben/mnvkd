/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_signal.h"
#include "vk_signal_s.h"

#include <string.h>
#include <setjmp.h>
#include <sys/errno.h>
#include <stdio.h>

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

void vk_signal_exec_jumper() {
    global_vk_signal.jumper(global_vk_signal.jumper_udata, global_vk_signal.siginfo_ptr, global_vk_signal.uc_ptr);
}
void vk_signal_exec_mainline() {
    global_vk_signal.mainline(global_vk_signal.mainline_udata);
}

int vk_signal_setjmp() {
    int rc;

    if (global_vk_signal.jumper == NULL) {
        errno = ENOENT;
        return -1;
    }
    if (global_vk_signal.mainline == NULL) {
        errno = ENOENT;
        return -1;
    }

    /* never actually returns from this point on */
#ifdef VK_SIGNAL_USE_SIGJMP
    rc = sigsetjmp(global_vk_signal.sigenv, 1);
#else
    rc = _setjmp(global_vk_signal.env);
#endif
    if (rc == 1) {
        vk_signal_exec_jumper();
    } else {
        vk_signal_exec_mainline();
    }
    return 0;
}

void vk_signal_set_handler(void (*handler)(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *handler_udata) {
    global_vk_signal.handler = handler;
    global_vk_signal.handler_udata = handler_udata;
}

void vk_signal_set_jumper(void (*jumper)(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr), void *jumper_udata) {
    global_vk_signal.jumper = jumper;
    global_vk_signal.jumper_udata = jumper_udata;
}
void vk_signal_set_mainline(void (*mainline)(void *mainline_udata), void *mainline_udata) {
    global_vk_signal.mainline = mainline;
    global_vk_signal.mainline_udata = mainline_udata;
}
void *vk_signal_get_mainline_udata() {
    return global_vk_signal.mainline_udata;
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
        case SIGILL:
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
        signal_ptr->siginfo_ptr = siginfo_ptr;
        signal_ptr->uc_ptr = uc_ptr;
#ifdef VK_SIGNAL_USE_SIGJMP
        siglongjmp(signal_ptr->sigenv, 1);
#else
        _longjmp(signal_ptr->env, 1);
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
    ignore.sa_sigaction = (void (*)(int, siginfo_t *, void *)) SIG_IGN; /* SIG_IGN is the simpler prototype */
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
    rc = sigaction(SIGILL, &action, 0);
    if (rc == -1) {
        return -1;
    }

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

#define stringize(sig) #sig
int vk_signal_get_siginfo_str(siginfo_t *siginfo_ptr, char *str, size_t size) {
    char *signame;
    char *sigdesc;
    char *sigcode;
    char *sigdetail;
    if (siginfo_ptr == NULL) {
        return snprintf(str, size, "No Signal.\n");
    }
    signame = "Unknown";
    sigdesc = "Unknown signal.";
    sigcode = "";
    sigdetail = "";
    switch (siginfo_ptr->si_signo) {
        case SIGABRT: signame = stringize(SIGABRT); sigdesc = "Process abort signal."; break;
        case SIGALRM: signame = stringize(SIGALRM); sigdesc = "Alarm clock."; break;
        case SIGBUS:  signame = stringize(SIGBUS);  sigdesc = "Access to an undefined portion of a memory object."; break;
        case SIGCHLD: signame = stringize(SIGCHLD); sigdesc = "Child process terminated, stopped, or continued."; break;
        case SIGCONT: signame = stringize(SIGCONT); sigdesc = "Continue executing, if stopped."; break;
        case SIGFPE:  signame = stringize(SIGFPE);  sigdesc = "Erroneous arithmetic operation."; break;
        case SIGHUP:  signame = stringize(SIGHUP);  sigdesc = "Hangup."; break;
        case SIGILL:  signame = stringize(SIGILL);  sigdesc = "Illegal instruction."; break;
        case SIGINT:  signame = stringize(SIGINT);  sigdesc = "Terminal interrupt signal."; break;
        case SIGKILL: signame = stringize(SIGKILL); sigdesc = "Kill (cannot be caught or ignored)."; break;
        case SIGPIPE: signame = stringize(SIGPIPE); sigdesc = "Write on a pipe with no one to read it."; break;
        case SIGQUIT: signame = stringize(SIGQUIT); sigdesc = "Terminal quit signal."; break;
        case SIGSEGV: signame = stringize(SIGSEGV); sigdesc = "Invalid memory reference."; break;
        case SIGSTOP: signame = stringize(SIGSTOP); sigdesc = "Stop executing (cannot be caught or ignored)."; break;
        case SIGTERM: signame = stringize(SIGTERM); sigdesc = "Termination signal."; break;
        case SIGTSTP: signame = stringize(SIGTSTP); sigdesc = "Terminal stop signal."; break;
        case SIGTTIN: signame = stringize(SIGTTIN); sigdesc = "Background process attempting read."; break;
        case SIGTTOU: signame = stringize(SIGTTOU); sigdesc = "Background process attempting write."; break;
        case SIGUSR1: signame = stringize(SIGUSR1); sigdesc = "User-defined signal 1."; break;
        case SIGUSR2: signame = stringize(SIGUSR2); sigdesc = "User-defined signal 2."; break;
#ifdef SIGPOLL
        case SIGPOLL: signame = stringize(SIGPOLL); sigdesc = "Pollable event."; break;
#endif
        case SIGPROF: signame = stringize(SIGPROF); sigdesc = "Profiling timer expired."; break;
        case SIGSYS:  signame = stringize(SIGSYS);  sigdesc = "Bad system call."; break;
        case SIGTRAP: signame = stringize(SIGTRAP); sigdesc = "Trace/breakpoint trap."; break;
        case SIGURG:  signame = stringize(SIGURG);  sigdesc = "High bandwidth data is available at a socket."; break;
        case SIGVTALRM: signame = stringize(SIGVTALRM); sigdesc = "Virtual timer expired."; break;
        case SIGXCPU: signame = stringize(SIGXCPU); sigdesc = "CPU time limit exceeded."; break;
        case SIGXFSZ: signame = stringize(SIGXFSZ); sigdesc = "File size limit exceeded."; break;
    }

    switch (siginfo_ptr->si_signo) {
        case SIGILL:
            switch (siginfo_ptr->si_code) {
                case ILL_ILLOPC: sigcode = stringize(ILL_ILLOPC); sigdetail = "Illegal opcode."; break;
                case ILL_ILLOPN: sigcode = stringize(ILL_ILLOPN); sigdetail = "Illegal operand."; break;
                case ILL_ILLADR: sigcode = stringize(ILL_ILLADR); sigdetail = "Illegal addressing mode."; break;
                case ILL_ILLTRP: sigcode = stringize(ILL_ILLTRP); sigdetail = "Illegal trap."; break;
                case ILL_PRVOPC: sigcode = stringize(ILL_PRVOPC); sigdetail = "Privileged opcode."; break;
                case ILL_PRVREG: sigcode = stringize(ILL_PRVREG); sigdetail = "Privileged register."; break;
                case ILL_COPROC: sigcode = stringize(ILL_COPROC); sigdetail = "Coprocessor error."; break;
                case ILL_BADSTK: sigcode = stringize(ILL_BADSTK); sigdetail = "Internal stack error."; break;
            }
            break;
        case SIGFPE:
            switch (siginfo_ptr->si_code) {
                case FPE_INTDIV: sigcode = stringize(FPE_INTDIV); sigdetail = "Integer divide by zero."; break;
                case FPE_INTOVF: sigcode = stringize(FPE_INTOVF); sigdetail = "Integer overflow."; break;
                case FPE_FLTDIV: sigcode = stringize(FPE_FLTDIV); sigdetail = "Floating-point divide by zero."; break;
                case FPE_FLTOVF: sigcode = stringize(FPE_FLTOVF); sigdetail = "Floating-point overflow."; break;
                case FPE_FLTUND: sigcode = stringize(FPE_FLTUND); sigdetail = "Floating-point underflow."; break;
                case FPE_FLTRES: sigcode = stringize(FPE_FLTRES); sigdetail = "Floating-point inexact result."; break;
                case FPE_FLTINV: sigcode = stringize(FPE_FLTINV); sigdetail = "Invalid floating-point operation."; break;
                case FPE_FLTSUB: sigcode = stringize(FPE_FLTSUB); sigdetail = "Subscript out of range."; break;
            }
            break;
        case SIGSEGV:
            switch (siginfo_ptr->si_code) {
                case SEGV_MAPERR: sigcode = stringize(SEGV_MAPERR); sigdetail = "Address not mapped to object."; break;
                case SEGV_ACCERR: sigcode = stringize(SEGV_ACCERR); sigdetail = "Invalid permissions for mapped object."; break;
            }
            break;
        case SIGBUS:
            switch (siginfo_ptr->si_code) {
                case BUS_ADRALN: sigcode = stringize(BUS_ADRALN); sigdetail = "Invalid address alignment."; break;
                case BUS_ADRERR: sigcode = stringize(BUS_ADRERR); sigdetail = "Nonexistent physical address."; break;
                case BUS_OBJERR: sigcode = stringize(BUS_OBJERR); sigdetail = "Object-specific hardware error."; break;
            }
            break;
        case SIGTRAP:
            switch (siginfo_ptr->si_code) {
#ifdef TRAP_BRKPT
                case TRAP_BRKPT: sigcode = stringize(TRAP_BRKPT); sigdetail = "Process breakpoint."; break;
#endif
#ifdef TRAP_TRACE
                case TRAP_TRACE: sigcode = stringize(TRAP_TRACE); sigdetail = "Process trace trap."; break;
#endif
            }
            break;
        case SIGCHLD:
            switch (siginfo_ptr->si_code) {
                case CLD_EXITED: sigcode = stringize(CLD_EXITED); sigdetail = "Child has exited."; break;
                case CLD_KILLED: sigcode = stringize(CLD_KILLED); sigdetail = "Child has terminated abnormally and did not create a core file."; break;
                case CLD_DUMPED: sigcode = stringize(CLD_DUMPED); sigdetail = "Child has terminated abnormally and created a core file."; break;
                case CLD_TRAPPED: sigcode = stringize(CLD_TRAPPED); sigdetail = "Traced child has trapped."; break;
                case CLD_STOPPED: sigcode = stringize(CLD_STOPPED); sigdetail = "Child has stopped."; break;
                case CLD_CONTINUED: sigcode = stringize(CLD_CONTINUED); sigdetail = "Stopped child has continued."; break;
            }
            break;
#ifdef SIGPOLL
        case SIGPOLL:
            switch (siginfo_ptr->si_code) {
                case POLL_IN: sigcode = stringize(POLL_IN); sigdetail = "Data input available."; break;
                case POLL_OUT: sigcode = stringize(POLL_OUT); sigdetail = "Output buffers available."; break;
                case POLL_MSG: sigcode = stringize(POLL_MSG); sigdetail = "Input message available."; break;
                case POLL_ERR: sigcode = stringize(POLL_ERR); sigdetail = "I/O error."; break;
                case POLL_PRI: sigcode = stringize(POLL_PRI); sigdetail = "High priority input available."; break;
                case POLL_HUP: sigcode = stringize(POLL_HUP); sigdetail = "Device disconnected."; break;
            }
            break;
#endif
    }
    switch (siginfo_ptr->si_code) {
        case SI_USER: sigcode = stringize(SI_USER); sigdetail = "Signal sent by kill()."; break;
        case SI_QUEUE: sigcode = stringize(SI_QUEUE); sigdetail = "Signal sent by sigqueue()."; break;
        case SI_TIMER: sigcode = stringize(SI_TIMER); sigdetail = "Signal generated by expiration of a timer set by timer_settime()."; break;
        case SI_ASYNCIO: sigcode = stringize(SI_ASYNCIO); sigdetail = "Signal generated by completion of an asynchronous I/O request."; break;
        case SI_MESGQ: sigcode = stringize(SI_MESGQ); sigdetail = "Signal generated by arrival of a message on an empty message queue."; break;
    }

    return snprintf(str, size,
        "Signal %s: %s - Code %s: %s",
        signame,
        sigdesc,
        sigcode,
        sigdetail
    );
}

/* send async signal to sync storage */
void vk_signal_send(int signo) {
    global_vk_signal.signo = signo;
}
/* receive signal synchronously */
int vk_signal_recv() {
    int signo;
    signo = (int) global_vk_signal.signo;
    if (signo) {
        global_vk_signal.signo = 0;
    }
    return signo;
}

#ifdef VK_SIGNAL_TEST
#include <stdlib.h>
#include <stdio.h>

void logic(int *count_ptr) {
    volatile int rc;
    volatile char *ptr;
    char c;

    printf("count: %i\n", *count_ptr);
    if (++(*count_ptr) >= 10) {
        return;
    }

    if (*count_ptr % 2 == 0) {
        printf("division by zero\n");
        rc = 0;
        rc = 5 / rc;
    } else {
        printf("segfault\n");
        ptr = NULL - 1;
        c = *ptr;
    }
}

void test_jumper(void *jumper_udata, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr) {
    int rc;
    char buf[256];

    rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
    if (rc == -1) {
        return;
    }
    buf[rc - 1] = '\n';
    printf("Recovering from signal %i: %s\n", siginfo_ptr->si_signo, buf);

    logic((int *) jumper_udata);
}
void test_handler(void *handler_udata, int jump, siginfo_t *siginfo_ptr, ucontext_t *uc_ptr) {
    int rc;
    char buf[256];

    rc = vk_signal_get_siginfo_str(siginfo_ptr, buf, 255);
    if (rc == -1) {
        return;
    }
    buf[rc - 1] = '\n';
    printf("Caught signal %i with jump %i: %s\n", siginfo_ptr->si_signo, jump, buf);
}

void test_mainline(void *mainline_udata) {
    logic((int *) mainline_udata);
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
    vk_signal_set_mainline(test_mainline, &c);
    rc = vk_signal_setjmp();
    if (rc == -1) {
        return 1;
    }

    return 0;
}
#endif
