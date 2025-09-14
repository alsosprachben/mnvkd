// vk_test_isolate_thread2.c
// Demonstrates vk_isolate integrated at the vk_thread level via vk_main_local_isolate_init.

#define _GNU_SOURCE
#include "vk_thread.h"
#include "vk_main_local_isolate.h"
#include "vk_isolate.h"
#include "vk_isolate_s.h"

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

struct demo_state2 {
    vk_isolate_t *vk;
    int           step;
    struct vk_thread *that; /* set by actor at first entry */
};

static void scheduler_cb2(void *ud)
{
    struct demo_state2 *D = (struct demo_state2 *)ud;
    printf("[scheduler] resume step=%d\n", D->step);
    if (D->step == 0) {
        D->step = 1; /* next continuation run will be step 1 */
        /* Re-enqueue the same thread to run step 1 */
        if (D->that) {
            vk_ready(D->that);
            vk_proc_local_enqueue_run(vk_get_proc_local(D->that), D->that);
        }
    } else {
        puts("[scheduler] done.");
    }
}

/* Real vk_thread actor that runs inside the isolate */
static void thread_actor(struct vk_thread *that)
{
    struct { struct demo_state2 *D; } *self;
    vk_begin();
    /* Load D from copied argument and remember our thread pointer */
    self->D->that = that;

    /* Step 0: attempt a syscall; with SUD active, traps to SIGSYS, prints step=0 */
    {
        volatile pid_t p = getpid();
        (void)p;
    }
    /* Yield cooperatively so the next vk_isolate_continue will pick up step 1 */
    vk_yield(VK_PROC_YIELD);

    /* Step 1: perform an isolated syscall that completes (scoped allow) */
    vk_isolate_syscall_begin();
    (void)getpid();
    vk_isolate_syscall_end();
    puts("[actor] step1 syscall ok");
    vk_end();
}

int main(void)
{
    int rc;
    struct demo_state2 D = {0};
    vk_isolate_t *iso = vk_isolate_create();
    if (!iso) {
        fprintf(stderr, "vk_isolate_create failed\n");
        return 1;
    }
    D.vk = iso;
    D.step = 0;

    /* Register a simple privileged page to exercise masking */
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    void *priv = mmap(NULL, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (priv == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    struct vk_priv_region r[] = {
        { .addr = priv, .len = page, .prot_when_unmasked = PROT_READ | PROT_WRITE }
    };
    vk_isolate_set_regions(iso, r, 1);
    vk_isolate_set_scheduler(iso, scheduler_cb2, &D);

    printf("SUD active: %s\n", vk_isolate_syscall_trap_active(iso) ? "yes" : "no (mask-only)");
    puts("[main] starting actor step 0 — syscall attempt should trap (if SUD active)");

    /* Run inside local, isolated runner */
    rc = vk_main_local_isolate_init(iso, thread_actor, &D, sizeof(struct demo_state2 *), 34);

    munmap(priv, page);
    vk_isolate_destroy(iso);
    return (rc == -1) ? 1 : 0;
}
