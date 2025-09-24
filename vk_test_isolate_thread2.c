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
    vk_isolate_t      isolate;
    int               step;
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

    vk_end();
}

int main(void)
{
    int rc;
    struct demo_state2 D = {0};
    if (vk_isolate_init(&D.isolate) == -1) {
        fprintf(stderr, "vk_isolate_init failed\n");
        return 1;
    }
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
    vk_isolate_set_regions(&D.isolate, r, 1);
    vk_isolate_set_scheduler(&D.isolate, scheduler_cb2, &D);

    printf("SUD active: %s\n", vk_isolate_syscall_trap_active(&D.isolate) ? "yes" : "no (mask-only)");
    puts("[main] starting actor step 0 — syscall attempt should trap (if SUD active)");

    /* Run inside local, isolated runner */
    struct demo_state2 *arg = &D;
    rc = vk_main_local_isolate_init(&D.isolate, thread_actor, &arg, sizeof(arg), 34);

    munmap(priv, page);
    vk_isolate_deinit(&D.isolate);
    return (rc == -1) ? 1 : 0;
}
