// vk_isolate.h — opaque interface
#pragma once
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vk_isolate vk_isolate_t;      // opaque
typedef void (*vk_scheduler_cb)(void *ud);   // scheduler callback

// Create/destroy an isolation object (one per worker thread is typical).
// Returns NULL on failure.
vk_isolate_t *vk_isolate_create(void);
void          vk_isolate_destroy(vk_isolate_t *vk);

// Configure scheduler callback and user data (once, or update later).
// Returns 0 on success.
int vk_isolate_set_scheduler(vk_isolate_t *vk, vk_scheduler_cb cb, void *user_data);

// Describe privileged regions that must be masked during isolated runs.
// Regions are copied; safe to free the caller's array after this call.
// Returns 0 on success.
struct vk_priv_region; // forward decl (details hidden here)
int vk_isolate_set_regions(vk_isolate_t *vk,
                           const struct vk_priv_region *regions,
                           size_t nregions);

// Enter isolated timeslice for an actor continuation.
// The actor either calls vk_isolate_yield() or returns;
// if it returns without yielding, we force a yield.
void vk_isolate_continue(vk_isolate_t *vk,
                         void (*actor_fn)(void *),
                         void *actor_arg);

// Escape hatch back to the scheduler (legal from inside/outside isolation).
void vk_isolate_yield(vk_isolate_t *vk);

// Query whether syscall trapping is active on this platform/config.
bool vk_isolate_syscall_trap_active(const vk_isolate_t *vk);

#ifdef __cplusplus
}
#endif
