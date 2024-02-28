# M:N Virtual Kernel Daemon

## Synopsis

`mnvkd` is a soft-real-time application server framework for C, designed for:
 1. fault-tolerant,
 2. high-throughput,
 3. low-latency,
 4. high-efficiency,
 5. low-cost,
 6. distributed systems.

`mnvkd` is a C alternative to Erlang and Golang:
 1. providing run-time memory safety, but
 2. without garbage collection,
 3. without locking,
 4. without a custom language, and
 5. within an unfettered C environment.

`mnvkd` is a proof of concept for a novel memory protection:
 1. where memory protection and privilege separation can be done in-process in C.
 2. where a virtual kernel can be implemented in userland without a full kernel implementation, but rather a threading implementation using existing POSIX interfaces.
 3. scoped in virtual processes, a third scheduling layer between a virtual kernel and virtual threads: M:N:1 scheduling.
 4. The theory is simple:
    1. a regular threading library is simply an unprotected virtual kernel, therefore
    2. protecting a threading library makes a proper virtual kernel, therefore
    3. protecting a userland threading library (without kernel threads, with event-based userland scheduling) makes an extremely fast virtual kernel, therefore
    4. providing a memory-safe M:N processing solution.

`mnvkd` is a proof of concept for a novel deductive network poller:
 1. to optimally reduce the number of I/O system calls.
 2. where the poller knows when I/O will block, so it knows when to flush and poll. 
 3. where vector-based ring buffers are used to optimize I/O copying. 

`mnvkd` is a proof of concept for a novel memory layout:
 1. where structured programming can also involve a structured memory layout that aligns data-structure with code-structure, dramatically reducing cache misses.
 2. that reduces the need for large amounts of on-die cache, meaning that expensive server processors are no longer needed.
 3. that greatly simplifies userland memory protection and scheduling.

`mnvkd` is a proof of concept for partitioned scheduling:
 1. where data partitioning is extended to scheduling.
 2. That is, cache-missing linear access only needs to apply to the partitioned structure, the virtual process,
 3. meaning that cache-hitting, scanning access can be used locally, within the virtual process, to improve throughput.
 4. That is, both thread scheduling and memory protection can benefit from an encapsulating locality of reference,
 5. which is what makes in-process memory protection actually feasible.

### Soft-Real-Time

A soft-real-time system is a real-time system that runs on top of a non-real-time operating system, so it cannot satisfy hard deadlines (as a hard-real-time system can), but it uses the system as deterministically as possible, to:
 1. reduce timing jitter, to
 2. reduce the area under the curve of the distribution of the latency tail, to
 3. reduce the percentage of deadline timeouts, while increasing the throughput of opportunities per cost.

Such a system can perform in an environment with deadlines that demand very high throughput, where efficiency at a particular service level is critical. For example, it is ideal for any kind of API servicing with very demanding network performance requirements, such as:
 1. real-time bidding at extremely low cost.
 2. APIs with very short deadline contexts combined with auto-retries.
 3. distributed systems with service meshes or API gateways that are:
    1. microservice-based, or
    2. actor-based.
 4. in-network partner integrations.
 5. datacenter-to-datacenter integrations.
 6. even regular application serving or web serving where latency is critical.

That is, it is general-purpose, but capable of satisfying very special purposes. It is especially suited where existing tools are failing, like where third-party cloud or edge platforms are not monetarily feasible, and off-the-shelf alternative components are not available.

### Deductive Polling

All I/O is optimally buffered, knowing deductively when the system would or could block.
 1. By mirroring the state of the underlying OS resources, the high-level I/O operations know when to flush, and when to poll.
 2. The blocking logic is designed for edge-triggering. For example, writes are not polled for:
    1. until an `EAGAIN` error is encountered, or
    2. until it is known that an `EAGAIN` error _would be_ encountered.

 - On Linux:
    1. `epoll()` is used:
       1. in an edge-triggered manner, with a single `epoll_ctl()` that registers all I/O if/when the first block occurs, or
       2. in a one-shot manner, with an `epoll_ctl()` that registers each time a block occurs.
    2. If no blocks happen, no registration is needed, preventing unneeded system calls.
    3. or `io_submit()` is used:
       1. where `io_submit()` is used to submit registrations in a one-shot manner, and
       2. `io_getevents()` is used to receive the events.
       3. If not all events can be submitted by `io_submit()` then `io_getevents()` is used to drain events to make space for another iteration of `io_submit()`.
       4. In the future, read and write operations will also be deferred to this point, and batched with `io_submit()`. Also, there is an optimization to implement `io_getevents()` in userland that hasn't been done yet.
 - On BSD (including OS X):
    1. `kqueue()` is used by batching the necessary `EV_ONESHOT` (or `EV_DISPATCH` in "edge-triggered" mode) registrations with the waiting call, and
    2. the count of available bytes on the socket is forwarded all the way to the `struct vk_socket` so that the high-level I/O operations can know what I/O will succeed.
    3. Registrations are batched with the polling wait (if buffer space is available), so no additional system call overhead is typically needed for event registration.
 - On other POSIX systems:
    1. `poll()` is configured from the same block-list that is used by the other event drivers, and
    2. is used in a one-shot manner, like edge-triggering, although each edge-block needs to be re-registered on each poll call.
    3. This means that the smallest block-list needed is used.
    4. Registrations are batched with the polling wait, although all concurrently blocked events need to be re-registered on each poll call. Under moderate concurrency levels (that is, about under 1000), even under high load, this is actually faster than `epoll()`, due to its inability to batch registrations with the wait.

There is a compounding benefit in this environment to avoid polling, since it also lowers the overhead of context switching in-process, and the micro-process will continue proceeding with warm caches. This means that an entire request can often be handled in a single micro-process dispatch. Beyond that, an entire pipeline of multiple requests and responses can be handled in a single dispatch. This means that both _reactive_ and _batch_ operations have the _same overhead_.

### Lockless, Gracefully-Batched Overload

At capacity, dispatching degrades into aggregation, not contention. The lowering of latency directly increases throughput, the theoretical ideal.

In a lock-based system, when capacity is reached, resources need to be switched, rather than simply being enqueued. In this manner, a lock behaves like a queue of size 0.

In practice, the throughput of a lock-based system reflects off the top of minimum-latency capacity the way an alias frequency reflects off the top of the Nyquist frequency. That is, it has been observed that going beyond that capacity by `n`% will result in a throughput of `100-n`% of total capacity. 

However, in a lockless system, throughput continues to grow beyond mimumum-latency capacity, trading latency for throughput, where the reduction of the number of event-loops per request actually increases total capacity beyond the minimum-latency capacity. And if requests are being pipelined, an even-greater, more-linear increase is observed.

This means that it is actually ideal to load the system just beyond the mimimum-latency capacity, where the processing jitter causes some aggregations to occur, but where the lower-side of the jitter allows the event queue to regularly drain completely, where latency doesn't get a chance to compound. This alone has been observed to improve total throughput by about 50% even under a soft-real-time deadline constraint.

### I/O Vectoring

The `readv()` and `writev()` system calls allow passing in `struct iovec` buffers, which can have multiple segments. This is used to send both segments of a ring buffer in a single system call. Therefore, only one copy of the data is needed for a buffer, and the same two segments can be used for sending and receiving to/from the buffer.

The ring buffers are held in the micro-process micro-heap.
 1. It is possible to write custom manipulations of a ring buffer, to use a ring buffer directly as a backing for a parser or emitter.
 2. There are already methods for splicing directly between ring buffers.
 3. Ring buffers can theoretically be shared, the data structure already supporting this.

There are currently no zero-copy methods yet, but they aren't really needed. The `vk_readable()` and `vk_writable()` interface can be used to do whatever is desired. In fact, the `vk_accept()` call simply uses `vk_readable()` to handle blocking. It is dead simple to use `vk_writable()` with `sendfile()` without needing a special interface, so a custom method can be created in 3 lines of code. And on BSD, the socket state already available in the `struct vk_socket` already contains the number of available bytes in the socket, so it is straightforward to build an extremely sophisticated, custom blocking logic.

Think of `mnvkd` as a network programming toolkit for the rapid development of optimal servers.

### Stackless Threading Framework 

The framework is implementated as a minimal threading library integrated with a process-isolating userland kernel. Applications are composed of stackless coroutines grouped like micro-threads in sets of micro-processes that each span individual contiguous memory segments that form micro-virtual-memory-spaces.

These micro-processes are driven by a virtual kernel's event loop. Each set of micro-threads in a micro-process are cooperatively scheduled within a single dispatch of a micro-process, running until all micro-threads block or exit. Each micro-process maintains its own run queue and blocked queue for its own micro-threads, isolating micro-thread scheduling, and event polling from the virtual kernel. That is, each micro-process has its own micro-scheduling and micro-polling.

This code-local and data-local design has many benefits:
- Enables a truly soft-real-time system by virtually eliminating jitter and latency tails, by:
    - eliminating run-time garbage collection, and
    - eliminating scheduling interruptions.
- Provides for a ludicrous amount of data cache and instruction cache locality, enabling both:
    1. high-throughput (high cache hit rate), and
    2. low-latency (local cache usage, and deterministic cache flushing).
- Due to isolated processing, and timerless, tickless, non-interruptable, cooperative scheduling:
    - Computation problems are smaller, the scope of code and data being vastly reduced.
    - Processes are much easier to make deterministic.
    - Processor TLB flushes align with network dispatches.

The stackless coroutines are provided a blocking I/O interface between OS sockets and other coroutines. Under the hood, the blocking I/O ops are C macros that build state machines that execute I/O futures. The ugly future and blocking logic is hidden behind macros.

The macros effectively translate the procedural "threads" into optimal functional state machines. The compiler is able to inline I/O ops directly into a thread function, and the function is not left until the logical blocking operations are not yet satisfiable. The caches stay hot through the entire I/O dispatch, yet are flushed in between for safety and consistency. Cache side-channel attacks are simply avoided. The memory of the thread, and even the caches of the memory, are unavailable when the thread is not running.

### Memory Hierarchy

The micro-processes get their own micro-heap, and the kernel gets its own micro-heap. 

1. 0 or more process memory `struct vk_heap` containing:
	1. 0 or more `struct vk_thread`: stackless coroutine micro-threads beside
	   1. 1 or more `struct vk_socket`: stream I/O handle
	      1. 2 `struct vk_pipe` (one for each side): represents an FD or other `struct vk_socket`
		  1. 2 `struct vk_vectoring` (one for each side): the I/O buffer for either
	2. 1 `struct vk_proc_local`: thread-local micro-process state
2. 1 kernel memory `struct vk_heap` containing:
	1. 1 `struct vk_kern`: the global "virtual kernel" state beside
	   1. 1 `struct vk_signal`: signal handler state
	2. 1 `struct vk_fd_table`: the file descriptor table of:
		1. `VK_FD_MAX count` of `struct vk_fd`: holding network event state for each FD
		   1. 3 `struct vk_io_future`:
    3. `VK_KERN_PROC_MAX` of `struct vk_pool_entry`: sub-heaps referencing
	4. `VK_KERN_PROC_MAX` of `struct vk_proc`: kernel-local micro-process state

Structure scope:

1. General structures:
   - `vk_heap`: memory mapping, an facilities for protecting the mapping
   - `vk_stack`: stack allocation within a memory mapping
   - `vk_pool`: a pool of fixed sized heaps
   - `vk_pool_entry`: an entry for a heap within a pool
2. Kernel-local structures:
   - `vk_kern`: the kernel
   - `vk_fd_table`: the FD table
   - `vk_fd`: each FD
   - `vk_proc`: kernel-local process (in kernel heap)
   - `vk_io_future`: inter-process future, or inter-poller future
   - `vk_signal`: signal handling
3. Process-local structures:
   - `vk_proc_local`: process-local process (in process heap)
   - `vk_thread`: stackless coroutine thread
   - `vk_socket`: I/O between FDs, or between other `vk_socket`
   - `vk_pipe`: represents either an FD or an `vk_socket` for a single side
   - `vk_future`: intra-process future

This layout has the property that the kernel and each process each span a single contiguous memory segment. This property means that a single `mprotect()` call can change the memory visibility of a process or the kernel.

### Micro-Process Safety

`mnvkd` is pure C, but it also manages memory. Instead of managing all pointers and buffers, it manages the way an operating system does: by controlling the visibility of memory regions. The inspiration is how kernels have been ported to run on top of another kernel, called a [virtual kernel](https://en.wikipedia.org/wiki/Vkernel) or [user-mode kernel](https://en.wikipedia.org/wiki/User-mode_Linux). However, instead of re-implementing an entire [Virtual Memory Manager](https://en.wikipedia.org/wiki/Virtual_memory), it uses the `mprotect()` system call to configure the operating systems's already-existing virtual memory manager to change the access control flags of regions of process memory.

This allows the virtual kernel to protect virtual processes, but protecting the virtual kernel is more complicated because the virtual kernel needs to be referenced without the virtual processes having knowledge of its location.

The author plans for this to be a reference implementation for the author's [proposed design for system calls to protect privileged kernel memory](https://spatiotemporal.io/#proposalasyscallisolatingsyscallforisolateduserlandscheduling).

These mechanisms provide runtime memory protection using hardware facilities, much more efficiently than a garbage collector, and without the need for porting to a different programming paradigm for compile-time verification.

- Hardware traps that generate signals, like segmentation faults and instruction faults, when generated during the execution of a micro-thread, get delivered to the error handler of the micro-thread for a clean recovery.

- The socket buffers and coroutine-local memory are allocated from the micro-heap, not the stack, since the micro-threads are stack-less. The stack can still be used, because this is pure C, and all of C is available. But using the platform interfaces means that buffers are managed, or straight-forward.

- There are no pointer references from process memory to kernel memory. Processes are completely encapsulated. The process merely drives its threads until blocked, then lets the kernel flush information from process-local memory to the kernel-local process state, and use that to poll for events which unblock the process.

### M:N Processing

It is now understood that the best way to implement M:N threads is to implement partitioned M:1 threads on top of 1:1 OS threads or processes. This is demonstrated by the likes of Erlang and Golang. But those are implemented as new languages with managed memory, and stack-based threads.
 - Erlang's virtual machine and runtime can have overhead dozens of times slower than C, but its locality of reference removes resource contention at scale.
 - Golang is a vast improvement in efficiency, but the CSP model is internal-only (not integrated into networking), and the shared memory -- lack of partitioning processes -- limits locality of reference, increasing resource contention at high scale. Its work-stealing scheduler sacrifices latency for throughput.
 
That is:
 - Erlang starts off slow, and improves with scale.
 - Golang starts off fast, and degrades with scale.
 - `mnvkd` starts off fast like Golang (actually, faster than Golang), and improves with scale like Erlang.

`mnvkd` does this:
 - without a new language.
 - without a garbage collector.
 - with lock-less, stack-less micro-threads in micro-heaps built at compile time.
 - with integrated, first-class networking.
 - with event-based, non-blocking futures, but with a thread-like, blocking interface.
 - with userland-level process isolation supported by hardware traps used by the kernel, but _faster than threads_.

To be more specific:
1. `mnvkd` is merely a server platform: instead of requiring a completely new language, it is simply plain C with zero-overhead metaprogramming, and a minimal runtime. 
2. `mnvkd` coroutines provide two-level scheduling of logical micro-threads within micro-processes, grouping micro-process threads together into a single event dispatch, only switching memory context when blocked. This enables the tight coupling (low-overhead switching) of related threads (thread-level scheduling), while preserving micro-process isolation (process-level scheduling). This can be seen as M:N:1 threading, (M:1 threads on M:1 processes on 1:1 cores).
3. `mnvkd` provides, like Erlang, composable virtual sockets that can be bound to either physical file descriptors or logical pipes between micro-threads. Additionally, within a micro-process, reference-passing futures may be used. 
4. `mnvkd` provides high-level, bidirectional stream interfaces, like `<stdio.h>`, plus a `readline()` for easy text protocol processing. The socket buffers are optimally handled by zero-overhead macros. The interface is meant to reflect writing an `inetd` server in a high-level language, but with the performance of the under-the-hood, event-based futures.
5. `mnvkd` provides micro-heap memory protection at the userland level, using hardware facilities used by the kernel. The two-level scheduling means that context switches only happen at ideal times, when the entire micro-process is blocked.

### Object-Oriented C Style

Each object `struct vk_${object}` type has 3 files in the form:
 - `vk_${object}.h`: type method interfaces with opaque, forward-declared, incomplete types
 - `vk_${object}_s.h`: complete type struct declaration
 - `vk_${object}.c`: type method implementations

 This provides an object-oriented interface, but with [intrusive references](https://250bpm.com/blog:8/). Containers are based on the `#include <sys/queue.h>` interface, intrusive lists. This greatly reduces the memory allocation overhead, and makes it far easier to implement process isolation in userland, since all related memory can reside within a single contiguous memory micro-heap.

 1. A `vk_*_s.h` file may include other `vk_*_s_.h` files for each member that is a `struct` value, not a pointer to a `struct`. Each pointer will instead be an incomplete `struct`.
 2. A `vk_*.h` file will use incomplete `struct`s for each pointer parameter.
 3. A `vk_*.c` file will include each `vk_*.h` it needs, and the `vk_*_s.h` of its own object.
 4. Symbols that are pointers are postfixed with `_ptr` for clarity.
 5. Symbols in the global namespace are prefixed with `vk_`.
 6. There is no need for `const`. It will be deduced by the compiler.

When compiled, link-time-optimization (LTO) will unwrap the opaque values, and be able to optimize through the opaque interfaces, deducing any `const` by escape analysis. This provides a proper public/private object-oriented interface, while allowing large, complex, intrusive structures to be composed in fixed-sized, contiguous ranges of memory, for high locality of reference. That is, proper object-orientation without complex object lifecycle management, making the lifecycle of a structure inherently "structured" as in "structured programming".

Following is an example of two objects: `example` (`struct vk_example`) and `sample` (`struct vk_sample`), where `sample` contains a member that is a complete `example` type. However, the interface header files `vk_example.h` and `vk_sample.h` contain no complete types, providing a clean declaration of the public interface.

Object `example`:

`vk_example_s.h`:
```c
#ifndef VK_EXAMPLE_S_H
#define VK_EXAMPLE_S_H

struct vk_example {
	int val;
};

#endif
```

`vk_example.h`:
```c
#ifndef VK_EXAMPLE_H
#define VK_EXAMPLE_H

struct vk_example;

int vk_example_get_val(struct vk_example *example_ptr);
void vk_example_set_val(struct vk_example *example_ptr, int val);

#endif
```

`vk_example.c`:
```c
#include "vk_example.h"
#include "vk_example_s.h"

int vk_example_get_val(struct vk_example *example_ptr) {
	return example_ptr->val;
}
void vk_example_set_val(struct vk_example *example_ptr, int val) {
	example_ptr->val = val;
}
```

Object `sample`:

`vk_sample_s.h`:
```c
#ifndef VK_SAMPLE_S_H
#define VK_SAMPLE_S_H

#include "vk_example_s.h"

struct vk_sample {
	struct vk_example example;
};

#endif
```

`vk_sample.h`:
```c
#ifndef VK_SAMPLE_H
#define VK_SAMPLE_H

struct vk_sample;

int vk_sample_get_example(struct vk_sample *sample_ptr);
void vk_sample_set_example(struct vk_sample *sample_ptr, struct vk_example *example_ptr);

#endif
```

`vk_sample.c`:
```c
#include "vk_sample.h"
#include "vk_sample_s.h"
#include "vk_example.h"

int vk_sample_get_example(struct vk_sample *sample_ptr) {
	return &sample_ptr->example;
}
void vk_sample_set_example(struct vk_sample *sample_ptr, struct vk_example *example_ptr) {
	vk_example_set_val(&sample_ptr->example, vk_example_get_val(example_ptr));
}
```

### Structured Programming

The intrusive data structure hierarchy allows for structured programming of both data and code. Both data and code can be co-isolated into mini processes, leading to the high cache locality of both data and instruction caches. This enables extremely high vertical scale with easy partitioning for horizontal scale.

### Coroutines

Complete example echo service:
```c
#include "vk_thread.h"
#include "vk_service.h"

void echo(struct vk_thread *that) {
	int rc;

	struct {
		struct vk_service service; /* via vk_copy_arg() */
		size_t i;
		struct {
			char in[8192];
			char out[8192];
		} *buf; /* pointer to demo dynamic allocation */
	} *self;

	vk_begin();

	for (self->i = 0; ; self->i++) {
		vk_calloc(self->buf, 1); /* demo dynamic allocation in a loop */

		vk_readline(rc, self->buf->in, sizeof (self->buf->in) - 1);
		if (rc == 0 || vk_eof()) {
			vk_free();
			break;
		}

		self->buf->in[rc] = '\0';

		rc = snprintf(self->buf->out, sizeof (self->buf->out) - 1, "Line %zu: %s", self->i, self->buf->in);
		if (rc == -1) {
			vk_error();
		}

		vk_write(self->buf->out, strlen(self->buf->out));
		vk_flush();

		vk_free(); /* demo deallocating each loop */
	}

	vk_end();
}

#include <stdlib.h>
#include <netinet/in.h>

int main(int argc, char *argv[]) {
	int rc;
	struct vk_server *server_ptr;
	struct vk_pool *pool_ptr;
	struct sockaddr_in address;

	server_ptr = calloc(1, vk_server_alloc_size());
	pool_ptr = calloc(1, vk_pool_alloc_size());

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	vk_server_set_pool(server_ptr, pool_ptr);
	vk_server_set_socket(server_ptr, PF_INET, SOCK_STREAM, 0);
	vk_server_set_address(server_ptr, (struct sockaddr *) &address, sizeof (address));
	vk_server_set_backlog(server_ptr, 128);
	vk_server_set_vk_func(server_ptr, echo);
	vk_server_set_count(server_ptr, 1000);
	vk_server_set_page_count(server_ptr, 25);
	vk_server_set_msg(server_ptr, NULL);
	rc = vk_server_init(server_ptr);
	if (rc == -1) {
		return 1;
	}

	return 0;
}
```

`struct vk_thread`: Micro-Thread
  - `vk_thread.h`
  - `vk_thread_s.h`
  - `vk_thread.c`

The stackless coroutines are derived from [Simon Tatham's coroutines](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html) based on [Duff's Device](https://en.wikipedia.org/wiki/Duff%27s_device) to build stackless state-machine coroutines. But the coroutines in `mnvkd` have a novel enhancement: instead of using the `__LINE__` macro twice for each yield (firstly for setting the return stage, and secondly for the `case` statement to jump back to that stage), the `__COUNTER__` and `__COUNTER__ - 1` macros are used. This makes it possible to wrap multiple yields in higher-level macros. When using `__LINE__`, an attempt to have one macro call multiple yields will have all yields collapse onto a single line, so the multiple stages will have the same state number, resulting in multiple case statements with the same value, a syntax error.

This problem is resolved by using `__COUNTER__`. Although `__COUNTER__` is not standard C, virtually every compiler has this feature (and it is trivially implementable in a custom preprocessor), and this allows high-level blocking operations to be built on lower-level blocking operations. The result is a thread-like interface, but with the very low overhead of state machines and futures.

The coroutine state is accessible via `struct vk_thread *that`, and the state-machine state-variable is an anonymous `struct { /* local state declarations */ } *self` declared at the top of the coroutine. The `*self` value is allocated from the micro-heap by `vk_begin()`, along with the standard I/O socket, and each of these is freed by `vk_end()`.    

These coroutines are stackless, meaning that stack variables may be lost between each blocking op, so any state-machine state must be preserved in memory associated with the coroutine (`*that` or `*self`), not the C stack locals. However, in between `vk_*()` macro invocations, the C stack locals behave normally, *but assume they are re-uninitialized by a coroutine restart*.

Conceptually, this is very similar to an `async` and `await` language syntax, where closure variables are lost after the first `await`, and coroutines act like `async` functions. Instead of chains of `async` functions:
1. "syntactic sugar" can be built-up in layers of yielding macros, and
2. coroutines can yield to each other, primarily across syntactic sugar for:
    1. message passing via futures, and
    2. blocking operations with coroutine-local buffers, against both:
        1. intra-process logical sockets bound together in userland, and
        2. physical operating system sockets. 

Minimal Example:
```c
#include "vk_thread.h"
void example(struct vk_thread *that) {
    struct {
        /* state variable */
    } *self;
    vk_begin();
    /* stateful process */
    vk_end();
}
```

`vk_thread_mem.h`: Memory API
 - `vk_calloc()`: stack-based allocation off the micro-heap
 - `vk_calloc_size()`: like `vk_calloc()`, but with an explicit size
 - `vk_free()`: free the allocation at the top of the stack

 When allocations would pass the edge of the micro-heap, an `ENOMEM` error is raised, and a log entry notes how many pages the micro-heap would have needed for the allocation to succeed. Starting with one page of memory, and increasing as needed, makes it easy to align heap sizes with code memory usage before compile time. All allocations are page-aligned, and fragments are only at the ends of pages. No garbage nor fragments can accumulate over time. Any memory leak would be obvious. 

`vk_thread_cr.h`: Coroutine API
 - `vk_begin()`: start stackless coroutine state machine
 - `vk_end()`:  end stackless coroutine state machine
 - `vk_yield()`: yield stackless coroutine state machine, where execution exits and re-enters

`vk_thread_exec.h`: Execution API
 - `vk_play()`: add the specified coroutine to the process run queue
 - `vk_pause()`: stop the current coroutine
 - `vk_call()`: transfer execution to the specified coroutine: `vk_pause()`, then `vk_play()` 
 - `vk_wait()`: pause execution to poll for specified socket

`vk_thread_ft.h`: Future API
 - `vk_spawn()`: `vk_play()` a coroutine, sending it a message, the current coroutine staying in the foreground
 - `vk_request()`: `vk_call()` a coroutine, sending it a message, pausing the current coroutine until the callee sends a message back
 - `vk_listen()`: wait for a `vk_request()` message
 - `vk_respond()`: reply a message back to the `vk_request()`, after processing the `vk_listen()`ed message

### Exceptions

`vk_thread_err.h`: Exception API
 - `vk_raise()|vk_raise_at()`: raise the specified error, jumping to the `vk_finally()` label
 - `vk_error()|vk_error_at()`: raise `errno` with `vk_raise()`
 - `vk_finally()`: where raised errors jump to
 - `vk_lower()`: within the `vk_finally()` section, jump back to the `vk_raise()` that jumped
 - `vk_get_signal()|vk_get_signal_at()`: if `errno` is `EFAULT`, there may be a caught hardware signal, like `SIGSEGV`
 - `vk_clear_signal()`

Errors can yield via `vk_raise(error)` or `vk_error()`, but instead of yielding back to the same execution point, they yield to a `vk_finally()` label. A coroutine can only have a single finally label for all cleanup code, but the cleanup code can branch and yield `vk_lower()` to lower back to where the error was raised. High-level blocking operations raise errors automatically. 

The nested yields provide a very simple way to build a zero-overhead framework idiomatic of higher-level C-like languages, but with the simplicity and power of pure C. Nothing gets in the way of pure C. 

### Sockets

`struct vk_socket`: Userland Socket
  - `vk_socket.h`
  - `vk_socket_s.h`
  - `vk_socket.c`

`struct vk_pipe`: Userland Pipe
  - `vk_pipe.h`
  - `vk_pipe_s.h`
  - `vk_pipe.c`

Each coroutine may have a default socket object that represents its Standard I/O. This socket may be bound to physical file descriptors, or be logically entangled with other sockets, as inter-coroutine message passing channels. These bindings are represented as a Pipe that holds either a file descriptor or a pointer to a socket's receive or send queue. The same blocking ops that work against file descriptors also work across these inter-coroutine channels, in which case data is simply spliced to and from socket queues.

Alternatively, a coroutine may yield a future directly to another coroutine, passing a reference directly to memory, rather than buffering data in a socket queue. This is more in the nature of a future.

A parent coroutine can spawn a child coroutine, where the child inherits the parent's socket. Otherwise, a special "pipeline" child can be used, which, on start, creates pipes to the parent like a unix pipeline. That is, the parent's standard output is given to the child, and the parent's standard output is instead piped to the child's standard input.  A normal child uses `vk_begin()` just like a normal coroutine, and a pipeline child uses `vk_begin_pipeline(future)`, which sets up the pipeline to the parent, and receives an initialization future message. All coroutines end with `vk_end()`, which is required to end the state machine's switch statement, and to free `self` and the default socket allocated by `vk_begin()`.

I/O API in `vk_thread_io.h`:
 - `vk_read()`: read a fixed number of bytes into a buffer, or until EOF
 - `vk_readline()`: read a fixed number of bytes into a buffer, or until EOF or a newline
 - `vk_write()`: write a fixed number of bytes from a buffer
 - `vk_flush()`: block until everything written has been physically sent
 - `vk_eof()`: EOF has been reached
 - `vk_clear()`: clear EOF status
 - `vk_nodata()`: EOF has been reached, and all data has been received
 - `vk_hup()`: hang up writing -- EOF on receiving side
 - `vk_hanged()`: hang up status
 - `vk_read_splice()`: read into socket what has been sent into other socket
 - `vk_write_splice()`: write into socket what has been received into other socket
 - `vk_rx_close()`: close the read side of the socket
 - `vk_tx_close()`: close the write side of the socket
 - `vk_readable()`: wait for socket to be readable (poll event)
 - `vk_writable()`: wait for socket to be writable (poll event)
 - `vk_read_splice()`: read from one socket into another
 - `vk_write_splice()`: write to one socket from another
 - `vk_accept()`: accept a file descriptor from a listening socket, for initializing a new coroutine (used by `vk_service`)

For each `vk_*()` op, there is an equal `vk_socket_*()` op that operates on a specified socket, rather than the coroutine default socket.

Each blocking operation is built on:
1. `vk_wait()`, which pauses the coroutine to be awakened by the network poller,
2. the `vk_socket_*()` interface in `vk_socket.h`, which holds vectoring and block objects, consumed by:
2. the `vk_vectoring_*()` interface in `vk_vectoring.h`, which gives and takes data from higher-level I/O ring buffer queues, and
3. the `vk_block_*()` interface in `vk_socket.h`, which controls signals the lower-level, physical socket operations.

### Vectorings: I/O Vector Ring Buffers

`struct vk_vectoring`: Vector Ring
  - `vk_vectoring.h`
  - `vk_vectoring_s.h`
  - `vk_vectoring.c`

The underlying OS socket operations send and receive between "I/O Vectors" called `struct iovec`, each an array of memory segments. A ring buffer can be represented by 1 or 2 I/O vectors. A vectoring object holds the ring buffer, and maintains a consistent view of the ring buffer via I/O vector pairs representing each of the parts for transmitting and receiving. This provides an intrinsic I/O queue suitable for the micro-heap.

### Micro-Heaps of Garbage-Free Memory

`struct vk_stack`: Micro-Stack
  - `vk_stack.h`
  - `vk_stack_s.h`
  - `vk_stack.c`

`struct vk_heap`: Micro-Heap
  - `vk_heap.h`
  - `vk_heap_s.h`
  - `vk_heap.c`

A set of coroutines are grouped into a contiguous memory mapping, a micro-heap. Coroutines within the micro-process pass execution around within a single dispatch, only involving the memory in the micro-heap. When the micro-process is not running, its micro-heap has read access disabled until it restarts. 

Instead of using externally-linked containers, the system intrinsic lists, `#include <sys/queue.h>`, are rather used. As intrinsic lists, the container attributes are embedded directly into the elements. This requires being explicit about which set memberships an element may have, but keeps memory local and un-fragmented. 

Memory is allocated from the heap as a stack of pages, allocated and de-allocated hierarchically, in the paradigm of structured programming, in stack order (first allocated, last de-allocated). The memory lifecycle is much more suited to a stack than execution. An object has one life, but may easily be executed in cycles. Loops tend to be stack-ordered, so loops that reallocate objects can do so with no overhead nor fragmentation.

In fact, the generational aspect of memory acknowledged by modern generational garbage collection technique is a reflection of this stack-based order of memory allocation. So instead of using garbage collection, a process-oriented stack of memory is all that is needed. Generally, when memory life-cycles are not stack-ordered, there is concurrency, and each concurrent process should then get its own micro-process and stack-oriented memory heap.

### Micro-Processes and Intra-Process Futures

`struct vk_proc`: Global Micro-Process (outside micro-heap)
  - `vk_proc.h`
  - `vk_proc_s.h`
  - `vk_proc.c`

`struct vk_proc_local`: Local Micro-Process (inside micro-heap)
  - `vk_proc_local.h`
  - `vk_proc_local_s.h`
  - `vk_proc_local.c`

`struct vk_future`: Intra-Process Future
  - `vk_future.h`
  - `vk_future_s.h`
  - `vk_future.c`

Run and blocking queues are per-heap, forming a micro-process that executes until the run queue is drained, leaving zero or more coroutines in the blocking queue. That is, a single dispatch progresses the execution in the micro-process as far as possible, via execution intra-futures (internal to the process), then blocks. Each coroutine's execution status is one of the following:
1. currently executing,
2. in the micro-process run queue,
3. held as an intra-process future in a coroutine's state (another form of queue), or
4. held as an inter-process I/O future in the network poller's state (another form of queue).

### Micro-Poller and Inter-Process Futures

`struct vk_io_future`: Inter-Process Future
  - `vk_io_future.h`
  - `vk_io_future_s.h`
  - `vk_io_future.c`

Each I/O block forms an inter-process I/O future (external to the process). When a poller has detected that the block is invalid, and the coroutine may continue, the Inter-Process future allows it to reschedule the coroutine, re-enable access to its micro-process's micro-heap, then restart the micro-process.

### Kernel Network Event Loop

`struct vk_kern`: Userland Kernel
 - `vk_kern.h`
 - `vk_kern_s.h`
 - `vk_kern.c`

The network poller is therefore a privileged process that is effectively the kernel that dispatches processes from the run queue and the ready events from the blocked queue. There is [a proposed design for system calls to protect this privileged kernel memory](https://spatiotemporal.io/#proposalasyscallisolatingsyscallforisolateduserlandscheduling), forming a new type of isolating virtualization.

The kernel is allocated from its own micro-heap of contiguous memory that holds:
1. the file descriptor table,
2. the process table,
3. the heads of the run and blocked queues.

To improve isolation, when a process is executing, its change in membership of the kernel run queue and blocked queue is held locally in `struct vk_proc_local` until after execution, then the state is flushed outside execution scope to `struct vk_proc` members with head in `struct vk_kern`. Manipulating the queues involves manipulating list links on other processes, and only the kernel can manipulate other processes, so the linked-list is global, while mere boolean flags are held locally. This way, the micro-heaps only contain local references.

### File Descriptor Table

`struct vk_fd_table`: FD Table
 - `vk_fd_table.h`
 - `vk_fd_table_s.h`
 - `vk_fd_table.c`

`struct vk_fd`: FD network poller state
 - `vk_fd.h`
 - `vk_fd_s.h`
 - `vk_fd.c`

The file descriptor table holds the network event registration state, and I/O between the network poller, either `poll()`. `epoll()`, `io_submit()`, or `kqueue()`. The FDs can be flagged as:
 - dirty: A new network block exists, so state needs to be registered with the poller.
 - fresh: Events were returned by the poller, so the blocked process needs to continued.

The polling time lifecycle state is kept:
 1. pre: needs to be registered with the poller, "dirty" flushing to:
 2. post: currently registered with the poller, "fresh" flushing to:
 3. ret: dispatch to the process, to continue the blocked virtual thread.

A network poller device simply registers the dirty FDs, and returns fresh FDs. 

### Services

`struct vk_service`: represents a service's server, and accepted client sockets
 - `vk_service.h`
 - `vk_service_s.h`
   - `struct vk_server`: state of the server connection, and its configuration
     - `vk_server.h`
     - `vk_server_s.h`
     - `vk_server.c`:
       - `vk_server_init()`: initialize a server
       - `vk_server_socket_listen()`: used by `vk_service_listener()` to creating a listening socket
       - `vk_socket_listen()`: listen on the standard socket
   - `struct vk_accepted`: state of the accepted connection
     - `vk_accepted.h`
     - `vk_accepted_s.h`
     - `vk_accepted.c` 
 - `vk_service.c`:
   - `vk_service_listener()`: platform provided service listener

Stub socket server example:
```c
#include "vk_thread.h"
#include "vk_service.h"

void example(struct vk_thread *that) {
	int rc;

	struct {
		struct vk_service service; /* via vk_copy_arg() */
        /* ... */
	} *self;

	vk_begin();
    /* ... */
	vk_end();
}

#include <stdlib.h>
#include <netinet/in.h>

int main(int argc, char *argv[]) {
	int rc;
	struct vk_server *server_ptr;
	struct sockaddr_in address;

	server_ptr = calloc(1, vk_server_alloc_size());

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	vk_server_set_socket(server_ptr, PF_INET, SOCK_STREAM, 0);
	vk_server_set_address(server_ptr, (struct sockaddr *) &address, sizeof (address));
	vk_server_set_backlog(server_ptr, 128);
	vk_server_set_vk_func(server_ptr, example);
	vk_server_set_page_count(server_ptr, 25);
	vk_server_set_msg(server_ptr, NULL);
	rc = vk_server_init(server_ptr);
	if (rc == -1) {
		return 1;
	}

	return 0;
}
```
