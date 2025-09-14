# Isolation and Syscall Gating

This document describes the design and integration of `vk_isolate`, a small
isolation/scheduling helper used to run mnvkd actors in a cooperative,
preemptable “isolated” window. It is inspired by and aligns with the intent of
“a syscall isolating syscall for isolated userland scheduling”:

- https://spatiotemporal.io/hammer.html#proposalasyscallisolatingsyscallforisolateduserlandscheduling

At a high level, `vk_isolate` uses a combination of Linux Syscall User
Dispatch (SUD) where available and page protection via `mprotect()` to:

- Trap direct syscalls made by isolated code (SUD → SIGSYS), and
- Prevent access to privileged memory while isolated (PROT_NONE → SIGSEGV),
- Then bounce back into the scheduler to decide what to do next.

This lets the runtime keep actors fully cooperative without relying on signal-
unsafe work inside arbitrary signal handlers.


## Goals

- Ensure actor code performs I/O only via mnvkd coroutine primitives.
- Close the escape hatch: accidental direct syscalls or access to privileged
  memory should trap and yield to the scheduler.
- Keep implementation small and composable with existing `vk_signal`-based
  error handling.


## Relationship to the linked proposal

The linked proposal argues for a kernel-provided switch that isolates userland
execution so that any “forbidden” operations cause a trap into a supervisor/
scheduler, enabling userland scheduling and strict separation of concerns.

`vk_isolate` implements this idea on Linux using the existing kernel feature
Syscall User Dispatch (SUD):

- A per-thread, writable selector byte (`sud_switch`) is registered with the
  kernel via `prctl(PR_SET_SYSCALL_USER_DISPATCH, ...)`.
- When the selector is set to “block”, all syscalls from the thread cause
  `SIGSYS`. `vk_isolate`’s `SIGSYS` handler immediately re-enables syscalls,
  unmasks privileged pages, clears “isolated” state, and hands control to a
  scheduler callback.
- When the selector is “allow”, syscalls proceed normally.

That is effectively a practical instance of the “syscall isolating syscall”
primitive proposed in the article. When SUD is not available, `vk_isolate`
still enforces memory isolation and cooperatively yields, but pure syscalls
won’t trap.


## The `vk_isolate` API

Public interface (`vk_isolate.h`):

- `vk_isolate_t *vk_isolate_create(void);`
- `void vk_isolate_destroy(vk_isolate_t *vk);`
- `int vk_isolate_set_scheduler(vk_isolate_t *vk, vk_scheduler_cb cb, void *ud);`
- `int vk_isolate_set_regions(vk_isolate_t *vk, const struct vk_priv_region *r, size_t n);`
- `void vk_isolate_continue(vk_isolate_t *vk, void (*actor_fn)(void *), void *arg);`
- `void vk_isolate_yield(vk_isolate_t *vk);`
- `bool vk_isolate_syscall_trap_active(const vk_isolate_t *vk);`

Key concepts:

- Scheduler callback: a function you provide that the isolate will call when an
  isolated step needs to return to the scheduler (due to a trap or cooperative
  yield).
- Privileged regions: memory that must not be accessed while an actor runs in
  isolation (e.g., global mutable state shared across actors or services). These
  pages are set to `PROT_NONE` during isolation and restored when leaving.


## How it works

### Entering an isolated step

`vk_isolate_continue()` does the following for the current thread:

1. Marks this `vk_isolate_t` instance as current for the thread.
2. Sets all configured privileged pages to `PROT_NONE` (`mprotect`).
3. If SUD is supported, sets the per-thread SUD selector to “block”.
4. Runs the actor continuation function you provided.

If the actor returns without calling `vk_isolate_yield()`, `vk_isolate` will
force a yield on your behalf to preserve invariants.

### Yielding back to the scheduler

`vk_isolate_yield()` transitions back to scheduler mode:

1. Sets SUD selector to “allow”.
2. Restores privileged pages to their configured protections.
3. Clears the “isolated” flag.
4. Calls the scheduler callback.

### Trap paths

- Syscalls while isolated (SUD only): kernel sends `SIGSYS`.
  The handler re-enables syscalls, unmasks pages, and calls your scheduler.
- Access to privileged pages while isolated: kernel sends `SIGSEGV`.
  Today, this is handled by mnvkd’s `vk_signal` machinery (see below), which can
  route the fault into the current micro-thread’s error handler and then back to
  the scheduler.


## Privileged vs. isolated memory

You describe privileged pages with `vk_isolate_set_regions()` using an array of
`struct vk_priv_region { void *addr; size_t len; int prot_when_unmasked; }`.

- When isolated: those pages are set to `PROT_NONE`.
- When not isolated: pages are restored to `prot_when_unmasked` (commonly
  `PROT_READ|PROT_WRITE`).

This creates a clear membrane around shared runtime structures so that an actor
cannot peek or mutate privileged state without first transitioning back to the
scheduler.


## Interaction with `vk_signal`

mnvkd already centralizes signal handling with `vk_signal` and integrates it
with the coroutine runtime (`vk_kern`, `vk_thread_*`). In this model:

- Hardware faults like `SIGSEGV` caused by page access on protected regions are
  surfaced through `vk_signal` and delivered to the current micro-thread’s error
  handler, which can cleanly unwind and yield.
- `vk_isolate` currently installs a narrow `SIGSYS` handler directly to handle
  SUD traps. This handler does not perform heavy work; it only flips the SUD
  gate and unmasks pages, then defers to the configured scheduler callback.

This split keeps `SIGSYS` handling extremely small and signal-safe while
continuing to leverage `vk_signal` for richer fault reporting, backtraces, and
micro-thread recovery.

A future unification could route `SIGSYS` through `vk_signal` too (e.g., by
registering the handler via `vk_signal_set_handler`), provided we maintain the
same signal-safety guarantees.


## Integration in mnvkd

`vk_isolate` is designed to be used per worker thread, wrapping the execution of
actor continuations:

- Create one `vk_isolate_t` per worker thread and set its scheduler callback to
  the worker’s run-queue wakeup (e.g., a function that reschedules the current
  actor or yields to the accept loop).
- Before running a micro-thread step, call `vk_isolate_continue(vk, step, arg)`.
  Inside the step, cooperative operations use vk’s coroutine I/O macros. Any
  direct syscall attempts (when SUD is active) or access to privileged pages
  will trap and return control to the scheduler.
- When an actor is ready to give up its timeslice, call `vk_isolate_yield(vk)`.

Typical privileged-page candidates include:

- Global, mutable service state (shared maps, connection tables, fd registries).
- Control-plane mailboxes and run-queues.
- Stats/metrics arenas updated only by scheduler code.

These can be grouped and registered once at worker init via
`vk_isolate_set_regions()`.


## Portability and fallback

- SUD support: Requires Linux ≥ 5.11 (supported on x86_64 and some other
  architectures). When unsupported, `vk_isolate_syscall_trap_active()` returns
  false. In that case, syscalls inside actors will not trap, but privileged page
  protection still applies and `vk_isolate_continue()` will still perform a
  cooperative yield when the actor returns.
- Other platforms: The memory protection scheme works anywhere with
  `mprotect()`. The SIGSYS path simply won’t be used.

### Modes and overhead

- Trap (default): Attempts SUD; if available, actors cannot perform syscalls.
  Memory regions are still masked during actor steps.
- Memory-only: Force memory protection without syscall masking by setting
  `VK_ISOLATE_MODE=memory` (or `VK_ISOLATE_SUD=0`). This protects privileged
  runtime memory from actor code but does not prevent syscalls. Useful for
  performance comparisons with language-level memory safety (e.g., GC systems).
- Controls:
  - `VK_ISOLATE_SUD=0`: disable SUD even if supported.
  - `VK_ISOLATE_MODE=memory`: force memory-only isolation.
  - `VK_ISOLATE_ALLOW_LIBC=1`: include libc text in SUD allowlist (default
    allows libc to keep signal-handling/logging reliable).


## Limitations and notes

- Allowlist region: SUD requires an always-allowed code range. `vk_isolate`
  auto-detects libc’s text range via `dl_iterate_phdr()` and registers it.
  Depending on distribution, you may also need to ensure vDSO and other runtime
  stubs remain allowed (the current approach works well on mainstream Linux).
- Actors must still use mnvkd’s coroutine I/O APIs. SUD is a guardrail, not a
  substitute for proper cooperative I/O.
- The privileged mask is coarse: it operates on whole pages. Group related
  structures to minimize false positives.


## I/O While Isolated: open issue and options

Today, actor code invokes I/O via the coroutine helpers in `vk_thread_io.h`,
which drive the underlying implementations in `vk_socket.c` and friends
(`vk_vectoring_*`, poller drivers, etc.). These ultimately perform syscalls
(`read`, `write`, `accept`, `shutdown`, poller ops) in the same call path as
the actor code. With SUD gating enabled, this means:

- On SUD-capable kernels, these syscalls will trap (`SIGSYS`) if attempted
  while the thread is in the isolated window.
- On non-SUD platforms, syscalls will proceed even while isolated; only memory
  is isolated.

There are two main directions to address this cleanly:

1) Whitelist/unmask runtime I/O
   - Intent: Allow the “virtual kernel” I/O code paths to make syscalls while
     the actor remains otherwise isolated.
   - Approaches:
     - SUD allowlist by code region: Arrange for vk’s I/O path to live in a
       contiguous text segment and register that as the always-allowed region
       when enabling SUD. This is fragile across builds/relocations and not as
       portable.
     - Scoped gate toggling: Inside the I/O path (e.g., around
       `vk_vectoring_read/write`, accept, poller ops), temporarily set the SUD
       selector to “allow”, optionally unmask privileged pages required by the
       runtime, perform the syscall, then restore isolation. This is robust and
       keeps enforcement for non-I/O syscalls attempted by actor code.

2) Virtual kernel mode-switch (recommended)
   - Intent: Treat I/O as a boundary crossing into a controlled “kernel mode”.
   - Mechanism:
     - Introduce a small pair of helpers (names illustrative):
       - `vk_isolate_enter_kernel(vk)`: allow syscalls, unmask privileged pages
         needed by the runtime.
       - `vk_isolate_leave_kernel(vk)`: re-block syscalls, remask privileged pages.
     - Bracket all direct syscalls in the vk runtime (e.g., in `vk_socket.c`
       and the poll drivers) with these helpers. The coroutine-facing macros in
       `vk_thread_io.h` remain unchanged but are safe: the I/O path performs a
       mode-switch on entry and exit.
   - Advantages:
     - Precise: approved I/O runs with syscalls enabled; actor code remains gated.
     - Portable: works with and without SUD; on platforms without SUD this
       still ensures privileged memory is unmasked only for the minimal duration.
     - Testable: the boundaries are explicit and narrow.

Isolation levels (proposed):

- Level 0: Memory-only isolation (current fallback when SUD unavailable).
- Level 1: SUD gating + scoped kernel mode around vk runtime I/O (recommended default).
- Level 2: Strict allowlist by code region for syscalls (advanced; requires
  careful layout and build support, useful for hardened deployments).

In all cases, accidental direct syscalls from actor code remain gated and trap
to the scheduler, preserving cooperativity and invariants.


## Mode-Switch Design (detailed)

You’re right: in mnvkd the I/O path operates on per-process (`vk_proc`) heaps
via `vk_socket` ring buffers, so with no SUD support we do not need to unmask
any privileged pages to perform I/O. We can keep privileged memory masked and
only allow syscalls in a controlled scope, validating that arguments are within
the process’s isolated bounds and that FDs are owned by the process.

Design summary:

- Two toggles: syscall gate vs. memory mask.
  - Syscall gate: SUD selector allow/block.
  - Memory mask: `mprotect()` on privileged regions.
- Kernel mode-switch for runtime I/O only opens the syscall gate; it does not
  unmask privileged pages. Actor code remains isolated by both mechanisms.
- Preflight validations enforce that syscalls respect process isolation.

Proposed helpers (header-only, very small):

- `vk_isolate_enter_kernel()`
  - If SUD is active and we are in an isolated window, set selector to ALLOW.
  - Track a per-thread depth counter to support nesting; first enter flips the
    gate, nested enters just increment depth.
- `vk_isolate_leave_kernel()`
  - Decrement depth; when it becomes zero and we are still in an isolated
    window, set selector to BLOCK again.
- These helpers do not unmask privileged pages.

Preflight validations (performed just before each syscall):

- IOV bounds: Ensure iovecs point inside the current process heap.
  - Use the current `vk_proc`’s `vk_heap.mapping.retval` and `.len` to check
    that `ring->buf_start` and computed iovecs are within range.
  - `vk_vectoring` already validates internal coherence
    (`vk_vectoring_validate`); the additional check confirms the buffer lives
    inside the process’s heap.
- FD ownership: Ensure the FD is owned by the current process.
  - Use the fd-table entry for the numeric FD to verify `allocated` and that
    `vk_fd.proc_id == vk_proc.proc_id`.
  - This matches mnvkd’s invariant that a kernel FD can only coherently be
    owned by one `vk_proc`.

Where to bracket and validate:

- Syscalls occur in `vk_vectoring.c`: `accept`, `readv`, `writev`, `close`,
  `shutdown` (and in poll drivers).
- Option A: bracket at `vk_socket.c` handlers (`vk_socket_handle_*`) which have
  access to the blocked thread (`socket_ptr->block.blocked_vk`) to fetch the
  current `vk_proc` for validation.
- Option B: bracket inside `vk_vectoring_*` functions and obtain the `vk_proc`
  via a light TLS pointer set when entering `vk_isolate_continue()`.
- For pollers (`epoll_wait`/`kevent`/`poll`), bracket in the driver code where
  the kernel calls are made; these calls do not touch privileged pages and only
  need the syscall gate opened during the call.

Fallback behavior (no SUD):

- `vk_isolate_enter_kernel()` becomes a no-op; validations still run.
- Privileged memory remains masked; since I/O uses only per-process heaps, this
  is safe and preserves isolation.

Error path if validation fails:

- Treat as an isolation violation:
  - Set `errno = EFAULT` or `EPERM` as appropriate.
  - Surface via `vk_signal` (or raise in the coroutine) to unwind and return to
    the scheduler. Optionally increment a counter and log with minimal detail.
  - Do not attempt the syscall when validation fails.

Implementation phases:

1) Add header-only helpers in `vk_isolate.h` (backed by TLS in `vk_isolate.c`):
   - depth-tracked `enter/leave kernel` that only toggles SUD.
2) Add validation utilities:
   - `vk_isolate_validate_heap_range(void *ptr, size_t len, struct vk_heap*)`.
   - `vk_isolate_fd_is_owned(int fd, struct vk_kern*, size_t proc_id)`.
3) Bracket syscalls:
   - In `vk_socket.c` handlers around calls into `vk_vectoring_*` (preferred),
     gather `that = socket_ptr->block.blocked_vk`, derive `proc`, validate FD
     and iovecs, then `enter/leave kernel` around the `vk_vectoring_*` call.
   - In poll drivers where the kernel call is made.
4) Tests:
   - Unit: negative tests for iovec out-of-bounds and cross-proc FD use.
   - Integration: demo that actor I/O works under SUD with pages still masked.
5) Metrics/observability (optional): counters for traps, violations, and mode
   switch counts per thread/process.

Security model recap:

- Actor code runs with both memory masking and syscall gating.
- Runtime I/O performs a narrow mode-switch to allow syscalls only, keeping
  privileged pages masked. Validations ensure data and FDs belong to the active
  process.
- On platforms without SUD, the validations still enforce boundaries; memory
  isolation remains in effect.


## Example

Build and run the included demo (Linux):

```
cc -O2 -Wall -ldl -o vk_test_isolate vk_isolate.c vk_test_isolate.c
./vk_test_isolate
```

Expected output on a SUD-capable kernel:

```
SUD active: yes
[main] starting actor step 0 — syscall attempt should trap (if SUD active)
[scheduler] resume step=0
[scheduler] resume step=1
[scheduler] done.
```

This shows the first step attempting a syscall, trapping via `SIGSYS` to the
scheduler, and then a cooperative second step followed by completion.


## Future work

- Route `SIGSYS` through `vk_signal` for unified fault reporting.
- Add optional “soft allowlists” for per-DSO regions beyond libc/vDSO.
- Provide helpers to register service-owned memory as privileged automatically
  from `vk_service`/`vk_server` initialization paths.
- Instrument with counters to surface trap rates and masked faults per actor.
