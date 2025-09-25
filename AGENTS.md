# Agent Instructions

This repository contains `mnvkd`, a C server-authoring toolkit built around a virtual kernel with coroutine, actor, service and application frameworks.

## Testing

- To run the project's tests with debug settings, use:
  - `./debug.sh bmake test`
- To run the tests with release/optimized settings, use:
  - `./release.sh bmake test`
- The project can also be built using CMake:
  - `cmake -S . -B build`
  - `cmake --build build`
  - `ctest --test-dir build` (if tests are enabled)
- For a quick debug run with a configurable timeout, use `./make_timeout.sh [-t seconds] [targets...]`, which wraps `./debug.sh bmake` and defaults to the `clean_all test test_servers` targets with a two-minute limit.

## Project Overview

Key areas of the project include:

- **Core modules**: Objects follow a three-file pattern described in [design.md#Object-Oriented C Style](design.md#object-oriented-c-style):
  `vk_<name>.h` exposes public interfaces with opaque types, `vk_<name>_s.h` defines the full structs, and `vk_<name>.c` provides the implementations. Examples include threading (`vk_thread.c`), networking (`vk_socket.c`), services (`vk_service.c`), and the main entry point (`vk_main.c`).
- **Tests and examples**: Files prefixed with `vk_test_` provide usage examples and unit tests.
- **Build scripts**: `debug.sh` and `release.sh` set up compilation flags for development or optimized builds.
- **Build systems**: Both BSD Make (invoked via `bmake`) and CMake are supported.

Always ensure tests pass before committing changes.

## Current Notes

Reference notes for the current focus areas:

- [aggregation.md](aggregation.md)
- [isolation.md](isolation.md)
- [io_submit.md](io_submit.md)

## Coroutines

See [threads.md](threads.md) for an overview of the coroutine model. Coroutines
are stackless and communicate via sockets, offering lockless, fully cooperative
concurrency reminiscent of Go's goroutines or Erlang's processes.

Coroutine functions share the signature `void name(struct vk_thread *that)`; any
other arguments are copied by the caller into the top of `self` before entry
using `vk_copy_arg`. `vk_begin()` allocates the coroutine's `self` state object
on the micro-heap. Each `vk_*()` function, implemented as a macro emitting a
state machine stage, may clear stack locals that are not stored in `self`.
Local variables must not live across a `vk_*()` call unless preserved in `self`;
otherwise the behavior is undefined.

I/O automation is handled by `vk_*()` macros (e.g. `vk_readline()`). Regular
functions may be called for non-I/O work, but they cannot perform I/O on their
own because they are not coroutines.

**Example**

```c
void example(struct vk_thread *that) {
    int rc = 0; // stack local
    struct {
        int limit;       // argument copied via vk_copy_arg
        int total;
        char line[64];
    } *self;
    vk_begin();                       // allocates `self`

    vk_readline(rc, self->line, sizeof(self->line) - 1);
    self->total += rc;                // persist across `vk_*()` calls

    vk_yield(VK_PROC_YIELD);          // `rc` is undefined after this

    vk_end();                         // frees `self`
}
```
