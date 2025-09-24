// vk_isolate.c — implementation
#define _GNU_SOURCE
#include "vk_isolate.h"
#include "vk_isolate_s.h"
#include "vk_signal.h"

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
    if (!vk || vk->nregions == 0) return;
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
    dl_iterate_phdr(phdr_cb, vk); // allow libc text range by default for handler safety
}

// ---- SUD toggles ----
static inline void sud_block_syscalls(vk_isolate_t *vk) {
    if (vk->sud_enabled && vk->sud_switch) {
        *vk->sud_switch = SYSCALL_DISPATCH_FILTER_BLOCK;
        __asm__ __volatile__("" ::: "memory");
    }
}
static inline void sud_allow_syscalls(vk_isolate_t *vk) {
    if (vk->sud_enabled && vk->sud_switch) {
        *vk->sud_switch = SYSCALL_DISPATCH_FILTER_ALLOW;
        __asm__ __volatile__("" ::: "memory");
    }
}

// ---- SIGSYS integration ----
static int vk_isolate_sigsys_hook(void *udata, siginfo_t *si, ucontext_t *uc);

static void setup_sigsys_support(vk_isolate_t *vk) {
    if (!vk) return;

    vk->have_altstack = false;
    vk->prev_altstack_saved = false;

    stack_t current;
    if (sigaltstack(NULL, &current) == 0) {
        vk->prev_altstack = current;
        vk->prev_altstack_saved = true;
        if (!(current.ss_flags & SS_DISABLE)) {
            // An alternate stack is already active; reuse it.
            vk_signal_set_sigsys_hook(vk_isolate_sigsys_hook, NULL);
            return;
        }
    }

    const size_t SSZ = 64 * 1024;
    void *stk = mmap(NULL, SSZ, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stk == MAP_FAILED) {
        vk_signal_set_sigsys_hook(vk_isolate_sigsys_hook, NULL);
        return;
    }

    memset(&vk->altstack, 0, sizeof(vk->altstack));
    vk->altstack.ss_sp = stk;
    vk->altstack.ss_size = SSZ;
    vk->altstack.ss_flags = 0;

    if (sigaltstack(&vk->altstack, NULL) == 0) {
        if (!vk->prev_altstack_saved) {
            memset(&vk->prev_altstack, 0, sizeof(vk->prev_altstack));
            vk->prev_altstack.ss_flags = SS_DISABLE;
            vk->prev_altstack_saved = true;
        }
        vk->have_altstack = true;
    } else {
        munmap(stk, SSZ);
        memset(&vk->altstack, 0, sizeof(vk->altstack));
    }

    vk_signal_set_sigsys_hook(vk_isolate_sigsys_hook, NULL);
}

static int vk_isolate_sigsys_hook(void *udata, siginfo_t *si, ucontext_t *uc)
{
    (void)udata;

    vk_isolate_t *vk = t_current;
    if (!vk) {
        return 0;
    }
    if (!vk->sud_enabled) {
        return 0;
    }
#ifdef SYS_USER_DISPATCH
    if (!si || si->si_code != SYS_USER_DISPATCH) {
        return 0;
    }
#endif
#ifndef SYS_USER_DISPATCH
    (void)si;
#endif

    if (t_in_sigsys_handler) {
        return 1;
    }
    t_in_sigsys_handler = true;

    vk_isolate_on_signal(vk);
    if (vk->cb) {
        vk->cb(vk->user_state);
    }

    t_in_sigsys_handler = false;
    (void)uc;
    return 1;
}

// ---- API impl ----

size_t vk_isolate_alloc_size(void) { return sizeof(vk_isolate_t); }

int vk_isolate_init(vk_isolate_t *vk)
{
    if (!vk) {
        errno = EINVAL;
        return -1;
    }

    memset(vk, 0, sizeof(*vk));

    setup_sigsys_support(vk);

    // Control via environment
    const char *sudenv = getenv("VK_ISOLATE_SUD");
    const char *mode   = getenv("VK_ISOLATE_MODE");
    int sud_user_disable = 0;
    if (sudenv && (*sudenv == '0' || *sudenv == 'n' || *sudenv == 'N')) {
        sud_user_disable = 1; // explicit legacy toggle
    }
    if (mode && (mode[0] == 'm' || mode[0] == 'M')) {
        // Force memory-only isolation (skip SUD setup)
        sud_user_disable = 1;
    }

    // Try SUD (Linux ≥ 5.11, x86); harmlessly fails elsewhere
    detect_libc_range(vk); // default: vDSO-only (empty); libc allowed only if explicitly enabled
    int rc = -1;
    if (!sud_user_disable) {
        size_t pgsz = (size_t)sysconf(_SC_PAGESIZE);
        void *page = mmap(NULL, pgsz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page != MAP_FAILED) {
            vk->sud_switch_page = page;
            vk->sud_switch_len  = pgsz;
            vk->sud_switch      = (volatile uint8_t *)page; // first byte used
            *vk->sud_switch = SYSCALL_DISPATCH_FILTER_ALLOW;
            __asm__ __volatile__("" ::: "memory");
            rc = prctl(PR_SET_SYSCALL_USER_DISPATCH,
                       PR_SYS_DISPATCH_ON,
                       (unsigned long)vk->libc_start,
                       (unsigned long)vk->libc_size,
                       (void *)vk->sud_switch);
        }
    }
    if (rc == 0) {
        vk->sud_supported = true;
        vk->sud_enabled   = true;
        sud_allow_syscalls(vk);
    } else {
        if (vk->sud_switch_page) {
            munmap(vk->sud_switch_page, vk->sud_switch_len);
            vk->sud_switch_page = NULL;
            vk->sud_switch = NULL;
            vk->sud_switch_len = 0;
        }
        vk->sud_supported = false;
        vk->sud_enabled   = false;
    }

    return 0;
}

void vk_isolate_deinit(vk_isolate_t *vk)
{
    if (!vk) return;
    if (vk->have_altstack) {
        stack_t restore;
        if (vk->prev_altstack_saved) {
            restore = vk->prev_altstack;
        } else {
            memset(&restore, 0, sizeof(restore));
            restore.ss_flags = SS_DISABLE;
        }
        (void)sigaltstack(&restore, NULL);

        munmap(vk->altstack.ss_sp, vk->altstack.ss_size);
        vk->have_altstack = false;
        vk->altstack.ss_sp = NULL;
        vk->altstack.ss_size = 0;
        vk->altstack.ss_flags = 0;
    }
    vk->prev_altstack_saved = false;
    if (vk->sud_switch_page) {
        munmap(vk->sud_switch_page, vk->sud_switch_len);
        vk->sud_switch_page = NULL;
        vk->sud_switch = NULL;
        vk->sud_switch_len = 0;
    }
    vk_signal_set_sigsys_hook(NULL, NULL);
    vk->cb = NULL;
    vk->user_state = NULL;
    vk->sud_enabled = false;
    vk->sud_supported = false;
    memset(vk->regions, 0, sizeof(vk->regions));
    vk->nregions = 0;
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
    if (!vk) {
        errno = EINVAL;
        return -1;
    }
    if (nregions > VK_ISOLATE_REGION_MAX) {
        errno = ENOSPC;
        return -1;
    }
    if (nregions > 0 && regions == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(vk->regions, 0, sizeof(vk->regions));
    vk->nregions = nregions;
    if (nregions > 0) {
        memcpy(vk->regions, regions, nregions * sizeof(*vk->regions));
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

void vk_isolate_on_signal(vk_isolate_t *vk)
{
    // Called from an async signal jumper path to ensure we are back in
    // scheduler mode with the gate open regardless of the trap source.
    if (!vk) return;

    // Allow syscalls again (no-op if SUD disabled) and unmask regions
    // back to their configured protections.
    sud_allow_syscalls(vk);
    mask_regions(vk, -1);
    t_in_isolated_window = false;
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

// (trap-only mode; no public gating helpers)
