# Aggregation in mnvkd

This document introduces two complementary design ideas that make mnvkd both safer and faster by aligning isolation with locality of reference:

- Mode Switch Aggregation: minimize transitions between the isolated actor window and the kernel window by aggregating them.
- I/O Aggregation: batch many pending I/O intents into a single kernel pass, including batched submission to async I/O backends.

The ideas are conceptual first; implementation details and concrete integration points follow afterwards.


## Concepts

### Why aggregate?

mnvkd treats application logic as actors (stackless coroutines) running in a protected "actor window". The runtime (a user‑mode kernel) performs I/O and coordination. Every time execution crosses between these windows there is overhead: toggling syscall filters, adjusting memory protections, touching schedulable queues, and possibly entering the poller.

Aggregation reduces that cost:

- Mode switches are amortized across many actors by exiting isolation once to process all work that must occur in the kernel window, then re‑entering isolation.
- I/O operations are validated and issued in batches rather than as a stream of individual syscalls, pushing overhead toward the noise floor.

The guiding principle is: spatial isolation is locality of reference. Secure is fast.


### Actor window vs. kernel window

- Actor window (isolated):
  - All privileged pages are `mprotect(PROT_NONE)`.
  - Syscalls are blocked (via Linux SUD or equivalent). Actors should not perform syscalls; they describe I/O intent.
  - Only pure computation and state mutation in actor heap are permitted.

- Kernel window (non‑isolated):
  - Privileged pages are unmasked.
  - Syscalls are allowed.
  - The scheduler validates arguments, issues I/O, reaps completions, updates queues, and then re‑enters isolation.


### Deferred I/O intent

Instead of immediately attempting I/O and blocking, actors record “I/O intents” (operation + buffer view + length) and yield. This adds a third coordination state:

- `RUN` – coroutine is executing in the actor window.
- `YIELD` – coroutine voluntarily yields and is ready to be rescheduled.
- `WAIT` – coroutine is waiting on poll/FD readiness that has been registered.
- `DEFER` – coroutine has recorded I/O intent that the kernel will validate and issue when aggregating.

When the run queue drains, the kernel window is entered once to process the entire `DEFER` queue, performing validation and issuing real I/O in batches. Actors are then promoted back to `RUN` or `WAIT` based on progress.


### I/O aggregation

In the kernel window, multiple deferred intents are coalesced into submission batches:

- For network FDs, contiguous ring segments are merged into `writev` or async `PWRITEV` requests.
- For reads, ring gaps are filled via `readv` or async `PREADV`.
- Readiness can be requested as part of the same batch (e.g., `IOCB_CMD_POLL`), folding "submit" and "wait" into a single pass.

This reduces per‑syscall overhead, improves cache locality, and minimizes isolates ↔ kernel toggles.


## Implementation Outline

This section maps the concepts to mnvkd’s existing components. Names are indicative; exact lines may differ as code evolves.

### What is implemented today

- The scheduler owns a dedicated `deferred_q`, recognises `VK_PROC_DEFER`, and services it through `vk_io_batch_sync` so each kernel pass batches the active OS-backed intents.
- High-level socket macros now default to `vk_defer()` for physical descriptors; the per-op macros populate the deferred queue and rely on the aggregator to drive progress instead of blocking inline.
- Virtual socket pairs (coroutines connected via pipes) continue to run in "immediate" mode: their intents execute synchronously during `vk_wait()`, preserving the old fast path and keeping coroutine rendezvous latency low.
- The isolation hand-off (actor window ↔ kernel window) happens once per aggregated pass; readiness bookkeeping and poll registration are deferred to that single transition.
- Special FD kinds (see `vk_fd_e.h`) such as `VK_FD_TYPE_SOCKET_LISTEN` follow the "message pipe" pattern: the kernel writes a structured record (e.g., `struct vk_accepted`) into the ring and the actor consumes it with a regular read macro. From the batching point of view these operations are just reads whose payload happens to be a control structure, so they drop naturally out of the same aggregation machinery once their op kinds (`VK_IO_ACCEPT`, etc.) are implemented.
- Recent hardening fixed the `EAGAIN`→`EOF` regression in the read completion path so that only successful zero-length completions flag `nodata`; transient readiness misses now requeue cleanly for another poll pass.

### Remaining work

- Replace the portable batcher with a backend-pluggable submitter (e.g., `io_uring`, `io_submit`) and add adaptive coalescing policies.
- Audit and migrate the remaining call sites that still depend on the inline `vk_wait()` fast path (pollread helpers, legacy tests) so the scheduler owns all physical I/O progression.
- Extend validation around mixed-direction forwarding, large scatter/gather bursts, and cross-proc fan-out; capture the edge cases in regression tests (e.g., explicit coverage for EPIPE/EAGAIN sequencing).
- Instrument aggregation metrics (batch size, mode-switch counts, completion latency, poll-spins) and surface them via the existing debug/log hooks.
- Tighten fairness controls in the batcher (per-proc round robin, per-FD byte quotas) to prevent chatty peers from starving cold queues.

### Isolation controls

- Syscall User Dispatch (SUD) gate:
  - Block syscalls in the actor window; allow only in the kernel window.
  - Make the libc "always‑allow" range opt‑in (default disabled) so that any stray syscall in the actor window traps to the scheduler instead of proceeding.

- Memory masking:
  - Configure privileged regions with `vk_isolate_set_regions()` (see `isolation.md`).
  - `mprotect(PROT_NONE)` before actor execution; restore protections for the kernel window.


- Scheduler and queues: `VK_PROC_DEFER` and the `deferred_q` are implemented; helpers exist to enqueue/dequeue deferred threads. Virtual sockets bypass the queue.

Loop structure in the isolated executor:

1) Drain the run queue by running actors inside the actor window.
2) When no thread is `RUN`, exit isolation once and process the deferred queue:
   - Validate intents.
   - Issue I/O (batch submit) and reap completions.
   - Update vector rings and move threads to `RUN` or `WAIT`.
3) Register pollers for residual blocked FDs and hand off to the poller.
4) Re‑enter isolation and continue with newly runnable threads.


- I/O macros → deferred intents: the macros now queue intents by default when the target FD is real. For virtual pipes we detect `fd < 0` and run the intent immediately, relying on `vk_wait()`/`vk_unblock()` to execute the fast path.


### Validation and safety

Before issuing real I/O, the kernel window validates all intents:

- Check ring coherence: `vk_vectoring_validate()`.
- Bounds and overflow: `vk_wrapguard_add/mult/safe_*` helpers.
- EOF/closed state: `vk_vectoring_has_eof/closed/nodata`.
- FD sanity and op compatibility.

Fail‑fast: if any check fails, raise an error in the owning coroutine via `vk_raise_at()` and promote it to run its error handler.


### Batch submission and reap

Backends (examples):

- Async Linux AIO (`io_submit`/`io_getevents`): pack multiple `preadv`/`pwritev`/`poll` requests per FD.
- `io_uring`: map intents to SQEs and let the kernel merge and complete.
- Portable fallback: coalesce per‑FD `writev`/`readv` and combine with a single `poll()` tick.

Batching policy:

- Coalesce adjacent ring segments and cap per‑batch iocbs.
- Preserve per‑FD ordering (e.g., HTTP/1.1 pipelining) by not reordering writes to the same socket.
- Apply fairness across threads and procs (round‑robin the deferred queue).


## Operational Characteristics

### Performance

- Fewer isolation exits: one kernel pass can satisfy many actors’ I/O.
- Lower syscall rate: vector coalescing + batched submission.
- Better cache locality: validations confined to kernel window; actor window is pure compute on hot data structures.

Suggested metrics:

- Mode switches per second and per request.
- I/O ops per batch; bytes per batch; completion latency percentiles.
- Actor time vs. kernel time per tick.


### Security

- Actor syscalls are blocked; attempts trap to the scheduler (no silent success).
- Privileged pages are inaccessible to actors (`PROT_NONE`).
- All I/O arguments are validated centrally before leaving the kernel window.


### Compatibility & rollout

- Keep a runtime toggle to re‑enable the libc allowlist for debugging (`VK_ISOLATE_ALLOW_LIBC=1`).
- Provide a gradual path: keep `vk_wait()`‑based immediate I/O behind a feature flag while migrating macros to `vk_defer()`.
- Fallback to non‑AIO batch path on platforms without async backends.


## Worked Example (Timeline)

1) Several actors `RUN` and generate write intents; each calls `vk_write(...)` which records a `DEFER` and yields.
2) Run queue drains → exit isolation once.
3) Validate all intents; build a single batch: a few `pwritev`s and a `poll`.
4) Submit; reap completions; update rings; move some threads to `RUN`, others to `WAIT`.
5) Register any remaining pollers; re‑enter isolation.
6) Newly runnable actors resume; no per‑intent mode flips were required.


## Next Steps (Implementation)

- **Hardening & tests** – Add regression coverage around EOF/EAGAIN/EPIPE sequencing, mixed read/write forwarding, and large scatter/gather batches so the deferred path remains correct under stress.
- **Observability** – Surface aggregation metrics (batch size, bytes/FD, mode switches, poll spins, latency buckets) via the existing debug channels and wire them into CI smoke tests.
- **Async backends** – Generalise the portable batcher behind a pluggable interface and add an io_uring submitter; share adaptive coalescing heuristics between backends.
- **API cleanup** – Finish migrating inline `vk_wait()` call sites to the deferred path (pollread helpers, legacy examples) and delete the remaining immediate-only code once parity tests pass.
- **Fairness controls** – Implement per-proc/ per-FD quotas in the batcher so noisy peers cannot monopolise a pass, and document the scheduling guarantees.


## Ring Buffer Sharing Summary

This consolidates how we share rings for zero extra copies while preserving isolation.

- Data vs. control:
  - Per‑FD RX/TX ring data pages live inside each `vk_proc` heap (unprivileged). Actors read/write only payload here.
  - Authoritative cursors/flags/FD state live in privileged control pages (masked in the actor window). Only the scheduler advances them after validation/completion.

- Isolation discipline:
  - Actor window: unmask only the current proc’s heap; kernel heap and other procs’ heaps remain `PROT_NONE`.
  - Kernel window: unmask kernel control and, per touched proc, unmask that proc to validate and issue I/O in an aggregated pass; remask before leaving.

- Zero‑copy flow:
  - TX: build iovecs from TX ring pages and issue writev/send (or async equivalents). Advance TX cursors on completion.
  - RX: issue recv/read into RX ring pages, then advance RX cursors. Optionally map RX as read‑only in actor window; kernel temporarily enables write during the pass.

- Pipeline forwarding without copy:
  - Build write iovecs that point directly to source RX ring pages; on completion, advance only the source RX side.

- Backends:
  - io_uring: register fixed files and fixed buffers and submit batched `SEND/RECV` (or `PWRITEV/PREADV`) across all procs; kernel DMAs into pinned pages while other heaps remain masked.
  - Portable: batch readiness (epoll/kqueue/libaio `POLL`), then coalesced `readv/writev` per ready FD; unmask only the owning proc while staging/issuing.


## io_submit on Sockets: Two Interpretations

There are two common readings of what libaio (`io_submit/io_getevents`) can do for sockets:

- Interpretation A (blog claim): submit `IOCB_CMD_PREAD/PWRITE` directly on TCP sockets to batch non‑blocking send/recv, completing inline if ready, `EAGAIN` otherwise.
  - Status: inaccurate for Linux AIO. Sockets do not implement native AIO read/write hooks; `PREAD/PWRITE` on a socket returns an error (e.g., `-EINVAL`/`-EOPNOTSUPP`). Inline completion semantics on sockets are a feature of io_uring, not libaio.

- Interpretation B (what works): batch socket readiness via `IOCB_CMD_POLL`, then perform coalesced non‑blocking data movement yourself.
  - Submit a large `POLL` batch via `io_submit` across sockets from all procs.
  - Reap readiness via `io_getevents` and, in the kernel window, perform `readv/writev` on the ready sockets in one aggregated pass; re‑arm `POLL` as needed.
  - For files/devices (preferably `O_DIRECT`), you can batch true async `PREAD/PWRITE` alongside the `POLL` batch.

Recommendation:
- Prefer io_uring for “one submit handles many socket send/recv” with inline completions and pinned user buffers (fixed files + fixed buffers).
- Maintain the portable libaio path by batching `POLL` and doing coalesced `readv/writev` in the aggregated kernel pass; you still get one actor→kernel switch and one submit/reap per tick.


### Observed behavior (Ubuntu 24.04, Linux 6.8)

On a test system:

```
uname -a
Linux ben 6.8.0-79-generic #79-Ubuntu SMP PREEMPT_DYNAMIC Tue Aug 12 14:42:46 UTC 2025 x86_64 x86_64 x86_64 GNU/Linux

cat /etc/issue
Ubuntu 24.04.3 LTS \n \l
```

We ran a minimal libaio test (`io_submit_test.c`) using `socketpair(AF_UNIX, SOCK_STREAM)` and observed:

- `io_submit(PREAD)` on a socket with no data: CQE `res = -11` (EAGAIN).
- `io_submit(PREAD)` on a socket with 3 bytes available: CQE `res = 3`.
- `io_submit(PWRITE)` on a likely-writable socket: CQE `res = 14` (bytes written).
- `io_submit(PWRITE)` after filling the send buffer: CQE `res = -11` (EAGAIN).

Interpretation:

- On this kernel, libaio accepted `IOCB_CMD_PREAD/PWRITE` against AF_UNIX sockets and completed them immediately with either progress or `-EAGAIN`. There was no in‑kernel wait; `io_getevents` returned promptly with an inline result.
- This effectively behaves as a “batched non‑blocking syscall” path for AF_UNIX sockets on this kernel: you can submit many operations and receive immediate CQEs with either bytes or `EAGAIN`.

Caveats:

- Historical guidance (and many kernels) do not support native AIO read/write for sockets; only `IOCB_CMD_POLL` is reliable across versions. AF_INET/IPv4 sockets were not tested here — behavior may differ.
- Because completions are immediate (or EAGAIN), there is still no implicit waiting in the kernel. You should still aggregate readiness (via `POLL`/epoll) to avoid re‑submitting large numbers of `EAGAIN` iocbs.

Action for mnvkd:

- Treat libaio `PREAD/PWRITE` on sockets as an opportunistic optimization where it works (e.g., AF_UNIX on this kernel), but structure the aggregator around batched readiness (`POLL`) plus coalesced non‑blocking `readv/writev` so behavior remains portable.
- Prefer io_uring when available for robust batched socket I/O with in‑kernel wait semantics.


## Implementation Checkpoint

This is where we left off and what comes next.

- Implemented (state‑first scaffolding):
  - `vk_io_op` (vk_io_op.h/.c/.s.h): single I/O intent with ring builders, iovecs, and coroutine ownership metadata.
  - `vk_io_queue` (vk_io_queue.h/.c/.s.h): per‑socket queues for staged intents (used by real FDs; virtual sockets short‑circuit).
  - `vk_io_exec` (vk_io_exec.h/.c): nonblocking `readv/writev` executor that classifies to DONE / NEEDS_POLL / ERROR and feeds completions back into rings.
  - `vk_io_batch_sync` (vk_io_batch_sync.h/.c): portable batcher that drains ≤1 op per FD, executes it, and reports completions and poll requirements.

- Semantics notes:
  - Partial completion (bytes < requested) is treated as “remainder would block”: state = `NEEDS_POLL`, `err = EAGAIN`; the batcher advances rings by the transferred amount and waits for readiness before retrying.
  - Pure virtual splice/forward stays immediate; those paths never enqueue `vk_io_op` records and continue to execute inside the actor window.

- Next steps (wire end‑to‑end):
  - Backend pluggability: add optional `io_submit` / `io_uring` submitters so the batcher can hand off to real async engines when available.
  - Finer-grained batching policy: coalesce adjacent intents across sockets, enforce fairness across procs, and instrument batch metrics.
  - Broaden macro coverage: audit remaining legacy call sites that still invoke `vk_wait()` directly and migrate them to deferred semantics where safe.
  - Timer integration: keep timerfd/timeout/io_uring TIMEOUT management in the poller but fold it into the aggregated kernel pass scheduling logic.

- Future backends:
  - libaio: batch `POLL`; true `PREAD/PWRITE` for files (`O_DIRECT`); optional socket PREAD/PWRITE where runtime probe shows inline bytes/EAGAIN; reap via `io_getevents`.
  - io_uring: fixed files + fixed buffers; batch `SEND/RECV` or `PREADV/PWRITEV`; `POLL_ADD`/`TIMEOUT` SQEs for readiness/timers.
  - Runtime selection with env overrides; sync batcher stays as baseline.
