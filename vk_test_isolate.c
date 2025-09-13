// test_vk_isolate.c
// Build:  cc -O2 -Wall -ldl -o vk_test_isolate vk_isolate.c vk_test_isolate.c
#define _GNU_SOURCE
#include "vk_isolate.h"
#include "vk_isolate_s.h"

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

// --- demo state ---
typedef struct {
    vk_isolate_t *vk;
    int           step;
    void         *priv_page;
    size_t        priv_len;
} demo_t;

static void scheduler_cb(void *ud);

// --- an "actor" that will first attempt a syscall (trapped),
// then run a second step that yields cooperatively
static void actor_fn(void *arg) {
    demo_t *D = (demo_t*)arg;
    if (D->step == 0) {
        // Attempt a syscall in isolation — should trigger SIGSYS -> scheduler
        pid_t p = getpid(); // trapped when SUD active
        // If SUD is not active, this will succeed; we still test masking
        (void)p;

        // Touch the privileged page (should SIGSEGV if PROT_NONE; we won't deliberately segfault here)
        // volatile char *c = (volatile char*)D->priv_page; *c = 1;

        // If we get here without trap, just fall through (the framework will force a yield)
    } else if (D->step == 1) {
        // Cooperative yield
        vk_isolate_yield(D->vk);
    }
}

static void scheduler_cb(void *ud) {
    demo_t *D = (demo_t*)ud;
    printf("[scheduler] resume step=%d\n", D->step);

    if (D->step == 0) {
        D->step = 1;
        vk_isolate_continue(D->vk, actor_fn, D);
    } else {
        puts("[scheduler] done.");
    }
}

int main(void) {
    demo_t D = {0};

    D.vk = vk_isolate_create();
    if (!D.vk) {
        fprintf(stderr, "vk_isolate_create failed\n");
        return 1;
    }

    // Allocate a "privileged" page and mark as region to be masked
    D.priv_len  = (size_t)sysconf(_SC_PAGESIZE);
    D.priv_page = mmap(NULL, D.priv_len, PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (D.priv_page == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    vk_isolate_set_scheduler(D.vk, scheduler_cb, &D);

    struct vk_priv_region regs[] = {
        { .addr = D.priv_page, .len = D.priv_len, .prot_when_unmasked = PROT_READ|PROT_WRITE }
    };
    vk_isolate_set_regions(D.vk, regs, 1);

    printf("SUD active: %s\n", vk_isolate_syscall_trap_active(D.vk) ? "yes" : "no (mask-only)");
    puts("[main] starting actor step 0 — syscall attempt should trap (if SUD active)");
    vk_isolate_continue(D.vk, actor_fn, &D);

    // Cleanup
    munmap(D.priv_page, D.priv_len);
    vk_isolate_destroy(D.vk);
    return 0;
}
