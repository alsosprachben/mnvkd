// vk_test_isolate_thread.c
// Runs the isolate test inside a vk_proc_local using vk_main_local_init.

#define _GNU_SOURCE
#include "vk_thread.h"
#include "vk_isolate.h"
#include "vk_isolate_s.h"

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

// vk_main_local entry (no public header)
int vk_main_local_init(vk_func main_vk, void *arg_buf, size_t arg_len, size_t page_count);

struct demo_state {
    vk_isolate_t  isolate;
    int           step;
    void         *priv_page;
    size_t        priv_len;
};

static void scheduler_cb(void *ud);
static void iso_actor(void *arg) {
    struct demo_state *D = (struct demo_state *)arg;
    if (D->step == 0) {
        volatile pid_t p = getpid();
        (void)p;
        // If SUD is active, this traps to scheduler_cb via SIGSYS.
        // If not, we fall through and framework will force a yield.
    } else if (D->step == 1) {
        // In trap-only mode, just resume cooperatively and yield back
        vk_isolate_yield(&D->isolate);
    }
}

static void scheduler_cb(void *ud) {
    struct demo_state *D = (struct demo_state *)ud;
    printf("[scheduler] resume step=%d\n", D->step);
    if (D->step == 0) {
        D->step = 1;
        vk_isolate_continue(&D->isolate, iso_actor, D);
    } else {
        puts("[scheduler] done.");
    }
}

static void test_isolate_thread(struct vk_thread *that) {
    int rc = 0;
    struct {
        struct demo_state d;
    } *self;
    vk_begin();


    if (vk_isolate_init(&self->d.isolate) == -1) {
        fprintf(stderr, "vk_isolate_init failed\n");
        vk_error();
    }

    // Allocate a privileged page and register it for masking
    self->d.priv_len = (size_t)sysconf(_SC_PAGESIZE);
    self->d.priv_page = mmap(NULL, self->d.priv_len, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (self->d.priv_page == MAP_FAILED) {
        perror("mmap");
        vk_error();
    }

    rc = vk_isolate_set_scheduler(&self->d.isolate, scheduler_cb, &self->d);
    if (rc != 0) {
        fprintf(stderr, "vk_isolate_set_scheduler failed\n");
        vk_error();
    }

    struct vk_priv_region regs[] = {
        { .addr = self->d.priv_page, .len = self->d.priv_len, .prot_when_unmasked = PROT_READ | PROT_WRITE }
    };
    rc = vk_isolate_set_regions(&self->d.isolate, regs, 1);
    if (rc != 0) {
        fprintf(stderr, "vk_isolate_set_regions failed\n");
        vk_error();
    }

    printf("SUD active: %s\n", vk_isolate_syscall_trap_active(&self->d.isolate) ? "yes" : "no (mask-only)");
    puts("[main] starting actor step 0 — syscall attempt should trap (if SUD active)");
    vk_isolate_continue(&self->d.isolate, iso_actor, &self->d);

    munmap(self->d.priv_page, self->d.priv_len);
    vk_isolate_deinit(&self->d.isolate);

    vk_end();
}

int main(void) {
    // drive a single local coroutine within a small private heap
    return vk_main_local_init(test_isolate_thread, NULL, 0, 34);
}
