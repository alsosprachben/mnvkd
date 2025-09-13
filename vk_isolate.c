// vk_isolate.c — implementation
#define _GNU_SOURCE
#include "vk_isolate.h"
#include "vk_isolate_s.h"

#include <errno.h>
#include <inttypes.h>
#include <link.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <ucontext.h>
#include <unistd.h>
#include <dlfcn.h>

// ---- SUD constants (define if missing) ----
#ifndef PR_SET_SYSCALL_USER_DISPATCH
#define PR_SET_SYSCALL_USER_DISPATCH 59
#endif
#ifndef PR_SYS_DISPATCH_ON
#define PR_SYS_DISPATCH_ON 1
#endif
#ifndef PR_SYS_DISPATCH_OFF
#define PR_SYS_DISPATCH_OFF 0
#endif
#ifndef SYSCALL_DISPATCH_FILTER_BLOCK
#define SYSCALL_DISPATCH_FILTER_BLOCK 0
#endif
#ifndef SYSCALL_DISPATCH_FILTER_ALLOW
#define SYSCALL_DISPATCH_FILTER_ALLOW 1
#endif

// ---- Thread-locals (one copy per TU, define here) ----
_Thread_local bool          t_in_isolated_window = false;
_Thread_local bool          t_in_sigsys_handler  = false;
_Thread_local vk_isolate_t *t_current            = NULL;

// ---- helpers: region masking ----
static void mask_regions(vk_isolate_t *vk, int prot) {
    if (!vk || !vk->regions) return;
    for (size_t i = 0; i < vk->nregions; ++i) {
        int p = (prot >= 0) ? prot : vk->regions[i].prot_when_unmasked;
        (void)mprotect(vk->regions[i].addr, vk->regions[i].len, p);
    }
}

// ---- resolve libc text range for SUD "always-allow" region ----
static int phdr_cb(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    vk_isolate_t *vk = (vk_isolate_t *)data;
    if (!info->dlpi_name || !*info->dlpi_name) return 0;
    if (!strstr(info->dlpi_name, "libc")) return 0;

    uintptr_t min_addr = UINTPTR_MAX;
    uintptr_t max_addr = 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD) continue;
        uintptr_t seg_start = info->dlpi_addr + ph->p_vaddr;
        uintptr_t seg_end   = seg_start + ph->p_memsz;
        if (seg_start < min_addr) min_addr = seg_start;
        if (seg_end   > max_addr) max_addr = seg_end;
    }
    if (min_addr < max_addr) {
        vk->libc_start = min_addr;
        vk->libc_size  = max_addr - min_addr;
        return 1; // stop
    }
    return 0;
}

static void detect_libc_range(vk_isolate_t *vk) {
    vk->libc_start = 0;
    vk->libc_size  = 0;
    dl_iterate_phdr(phdr_cb, vk);
}

// ---- SUD toggles ----
static inline void sud_block_syscalls(vk_isolate_t *vk) {
    if (vk->sud_enabled) {
        vk->sud_switch = SYSCALL_DISPATCH_FILTER_BLOCK;
        __asm__ __volatile__("" ::: "memory");
    }
}
static inline void sud_allow_syscalls(vk_isolate_t *vk) {
    if (vk->sud_enabled) {
        vk->sud_switch = SYSCALL_DISPATCH_FILTER_ALLOW;
        __asm__ __volatile__("" ::: "memory");
    }
}

// ---- SIGSYS handler ----
static void vk_sigsys_handler(int signo, siginfo_t *si, void *uctx) {
    (void)signo; (void)si;
    ucontext_t *uc = (ucontext_t *)uctx;

    vk_isolate_t *vk = t_current;
    if (!vk) return;

    if (t_in_sigsys_handler) return;
    t_in_sigsys_handler = true;

    // Open the gate & unmask privileged pages
    sud_allow_syscalls(vk);
    mask_regions(vk, -1);
    t_in_isolated_window = false;

    // Hand off to scheduler
    if (vk->cb) vk->cb(vk->user_state);

    t_in_sigsys_handler = false;
    (void)uc; // currently unused; keep for future logging
}

static void install_sigsys_handler(vk_isolate_t *vk) {
    // alt stack (64KB)
    const size_t SSZ = 64 * 1024;
    void *stk = mmap(NULL, SSZ, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stk != MAP_FAILED) {
        vk->altstack.ss_sp = stk;
        vk->altstack.ss_size = SSZ;
        vk->altstack.ss_flags = 0;
        if (sigaltstack(&vk->altstack, NULL) == 0) {
            vk->have_altstack = true;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = vk_sigsys_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if (vk->have_altstack) sa.sa_flags |= SA_ONSTACK;
    sigaction(SIGSYS, &sa, NULL);
}

// ---- API impl ----
vk_isolate_t *vk_isolate_create(void) {
    vk_isolate_t *vk = (vk_isolate_t*)calloc(1, sizeof(*vk));
    if (!vk) return NULL;

    install_sigsys_handler(vk);

    // Try SUD (Linux ≥ 5.11, x86); harmlessly fails elsewhere
    detect_libc_range(vk);
    int rc = prctl(PR_SET_SYSCALL_USER_DISPATCH,
                   PR_SYS_DISPATCH_ON,
                   (unsigned long)vk->libc_start,
                   (unsigned long)vk->libc_size,
                   &vk->sud_switch);
    if (rc == 0) {
        vk->sud_supported = true;
        vk->sud_enabled   = true;
        sud_allow_syscalls(vk);
    } else {
        vk->sud_supported = false;
        vk->sud_enabled   = false;
    }
    return vk;
}

void vk_isolate_destroy(vk_isolate_t *vk) {
    if (!vk) return;
    if (vk->have_altstack) {
        // no direct munmap via sigaltstack teardown; just unmap
        munmap(vk->altstack.ss_sp, vk->altstack.ss_size);
        vk->have_altstack = false;
    }
    free(vk->regions);
    free(vk);
}

int vk_isolate_set_scheduler(vk_isolate_t *vk, vk_scheduler_cb cb, void *user_data) {
    if (!vk) return -1;
    vk->cb = cb;
    vk->user_state = user_data;
    return 0;
}

int vk_isolate_set_regions(vk_isolate_t *vk,
                           const struct vk_priv_region *regions,
                           size_t nregions)
{
    if (!vk) return -1;
    free(vk->regions);
    vk->regions = NULL;
    vk->nregions = 0;
    if (nregions) {
        vk->regions = (struct vk_priv_region*)calloc(nregions, sizeof(*vk->regions));
        if (!vk->regions) return -1;
        memcpy(vk->regions, regions, nregions * sizeof(*vk->regions));
        vk->nregions = nregions;
    }
    return 0;
}

bool vk_isolate_syscall_trap_active(const vk_isolate_t *vk) {
    return vk && vk->sud_enabled;
}

void vk_isolate_continue(vk_isolate_t *vk,
                         void (*actor_fn)(void *),
                         void *actor_arg)
{
    if (!vk || !actor_fn) return;

    // Make this the current instance for the thread
    t_current = vk;

    // Enter isolated window
    mask_regions(vk, PROT_NONE);
    sud_block_syscalls(vk);
    t_in_isolated_window = true;

    // Run continuation
    actor_fn(actor_arg);

    // If actor returned without yielding, force yield to preserve invariants
    if (t_in_isolated_window) {
        vk_isolate_yield(vk);
    }
}

void vk_isolate_yield(vk_isolate_t *vk) {
    if (!vk) return;

    if (!t_in_isolated_window) {
        // Already in scheduler mode
        if (vk->cb) vk->cb(vk->user_state);
        return;
    }

    // Leave isolated window
    sud_allow_syscalls(vk);
    mask_regions(vk, -1);
    t_in_isolated_window = false;

    // Scheduler handoff
    if (vk->cb) vk->cb(vk->user_state);
}
