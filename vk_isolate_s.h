// vk_isolate_s.h — internal struct layout (non-opaque)
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <signal.h>
#include <ucontext.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Publicly referenced type (forward-declared in vk_isolate.h)
// This is exposed here so other mnvkd internals can inspect fields if needed.
typedef struct vk_isolate vk_isolate_t;

// Region of privileged pages to (un)mask around actor runs
struct vk_priv_region {
    void   *addr;
    size_t  len;
    int     prot_when_unmasked; // e.g., PROT_READ|PROT_WRITE
};

typedef void (*vk_scheduler_cb)(void *ud);

// Implementation state (per-instance)
struct vk_isolate {
    // scheduler
    vk_scheduler_cb cb;
    void           *user_state;

    // privileged regions
    struct vk_priv_region *regions;
    size_t                 nregions;

    // syscall user dispatch (SUD)
    bool        sud_supported;
    bool        sud_enabled;
    // selector byte checked on every syscall (in a dedicated writable page)
    volatile uint8_t *sud_switch;
    void       *sud_switch_page;
    size_t      sud_switch_len;
    uintptr_t   libc_start;
    size_t      libc_size;

    // alt signal stack
    stack_t     altstack;
    bool        have_altstack;
};

#ifdef __cplusplus
}
#endif
