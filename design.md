# M:N Virtual Kernel Daemon Design



#concurrency model

	- Consumer interfaces can be synchronous(request, response) or
    asynchronous(request and wait) - Producer interfaces can be synchronous(request, response) with or without reentrance (with a request queue), or asynchronous (request becomes a future)
- Interfaces should be synchronous unless parallelism is needed.
- coroutines are made reentrant with a per-coroutine queue.

#Resource Concurrency Interface

Queue internal to compute resources. Block external to compute resources. For example, golang uses fixed sized queues as channels between internal resource goroutines, but uses a single event loop for blocking on network resources, which are external.

#One Dynamic Queue

You only need one dynamic queue in a particular path. All the rest can be fixed if you block externally, to regulate black-pressure. This is the one piece missing from golang, although it is easy to implement using auto-growing slices, except for the realloc() overhead.

#Memory Layout

Each coroutine gets a heap of fixed pages backed by a single memory mapping. Within the heap, page-aligned, fixed-sized array allocations of a particular type may be allocated, until the heap pages are exhausted. 





## Platform Principles

### Horizontal Layers of Abstraction
To reduce the size of computational problems, software frameworks are often comprised of horizontal layers of abstraction. Data passes vertically through each layer, each layer processing all data, but at different stages of the process. Therefore, a "software stack" implements a "data pipeline". The use of a pipeline increases throughput, but the complexity of using many layers also increases latency.

### Mapping and Reducing: Big Data as Small Data
When "big data" is involved, a single big data process becomes comprised of many big data processes, one for each layer. The way to deal with a big data problem is to make it a small data problem. To do this, a "distributed system" is developed, where each layer is partitioned, data is "mapped" to each partition, then "reduced" on the other side. This terminology comes from the `map()` and `reduce()` functions common in the domain of functional programming. Sometimes this is called splitting and merging, or scattering and gathering. 

### Vertical Layers of Abstraction
Ideally, data should be mapped once at the beginning, then reduced once at the end, in contiguous vertical layers of abstraction, but often distributed systems end up unnecessarily re-reducing and re-mapping in between each layer. Such an architecture is often a symptom of a distributed system that unfortunately inherited the design of a non-distributed system that started out with horizontal layers of abstraction. Therefore, it can be beneficial for a distributed system to be designed from the beginning with vertical layers of abstraction. 

### Locality of Reference
With proper vertical layer partitioning, the horizontal layers can become local again. This completely changes the algorithmic conditions. The need for slow and costly cloud methods disappears:
  - No need for complex distributed hash tables just to store variables.
  - No need for complex service meshes to manage horizontal connections between systems.
  - No need for complex data lakes with complex batch processing latencies. 
  - No need for complex inter-lake journals with complex data pipeline management. 
  - No need for complex function dispatching "serverless" systems. 

Applications are small and efficient again, but exist within a modern horizontal resource backplane supporting local access, and this "locality of reference" mitigates the latency problem of distributed data pipelines.

### Platforms Manage Resources
In a computing system, the operating system's task is to manage the physical resources. The task of a platform's standard library is to interface with the operating system to expose the physical resources to design patterns, by implementing common algorithms optimized for resource management.

For example, the standard library implements various object containers, like sets, tables, lists, and queues, and provides interfaces for performing input and output with those containers. This explains why container implementations differ greatly across different platforms: the differences of resource management opinions. For example:
 - A platform that has garbage collection has containers with external references, often mixing object types in a single container, because the type information is associated with the object storage.
 - However, a platform with manually managed memory often uses internal references, and a container only supporting a single type, because the container is comprised of element references, and a fixed-sized head object.
 - A platform with stack-based threads needs a thread-aware memory allocator to deal with memory transactions.
 - However, an event-based platform with futures shares all memory, and all memory transactions are within a single thread.

### Frameworks Invert Control
In a typical functional computing environment, dependent platform code is linked as external functions called from application functions. The application is completely in control over its dependencies, and needs configuration changes to use different dependencies.

Rather than acting as a dependent library, a framework can invert control so that rather than being called by applications, the framework instead calls into applications, and the applications have no direct control over the framework. This simplifies the interface with the application, and makes it easier to unify configuration across applications.

## System Properties

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

However, in a lockless system, throughput continues to grow beyond minimum-latency capacity, trading latency for throughput, where the reduction of the number of event-loops per request actually increases total capacity beyond the minimum-latency capacity. And if requests are being pipelined, an even-greater, more-linear increase is observed.

This means that it is actually ideal to load the system just beyond the minimum-latency capacity, where the processing jitter causes some aggregations to occur, but where the lower-side of the jitter allows the event queue to regularly drain completely, where latency doesn't get a chance to compound. This alone has been observed to improve total throughput by about 50% even under a soft-real-time deadline constraint.

### I/O Vectoring

The `readv()` and `writev()` system calls allow passing in `struct iovec` buffers, which can have multiple segments. This is used to send both segments of a ring buffer in a single system call. Therefore, only one copy of the data is needed for a buffer, and the same two segments can be used for sending and receiving to/from the buffer.

The ring buffers are held in the micro-process micro-heap.
1. It is possible to write custom manipulations of a ring buffer, to use a ring buffer directly as a backing for a parser or emitter.
2. There are already methods for splicing directly between ring buffers.
3. Ring buffers can theoretically be shared, the data structure already supporting this.

There are currently no zero-copy methods yet, but they aren't really needed. The `vk_readable()` and `vk_writable()` interface can be used to do whatever is desired. It is dead simple to use `vk_writable()` with `sendfile()` without needing a special interface, so a custom method can be created in 3 lines of code. And on BSD, the socket state already available in the `struct vk_socket` already contains the number of available bytes in the socket, so it is straightforward to build an extremely sophisticated, custom blocking logic.

Think of `mnvkd` as a network programming toolkit for the rapid development of optimal servers.

### Stackless Threading Framework

The framework is implemented as a minimal threading library integrated with a process-isolating userland kernel. Applications are composed of stackless coroutines grouped like micro-threads in sets of micro-processes that each span individual contiguous memory segments that form micro-virtual-memory-spaces.

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
    - `vk_heap`: memory mapping, and facilities for protecting the mapping
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

`mnvkd` is pure C, but it also manages memory. Instead of managing all pointers and buffers, it manages the way an operating system does: by controlling the visibility of memory regions. The inspiration is how kernels have been ported to run on top of another kernel, called a [virtual kernel](https://en.wikipedia.org/wiki/Vkernel) or [user-mode kernel](https://en.wikipedia.org/wiki/User-mode_Linux). However, instead of re-implementing an entire [Virtual Memory Manager](https://en.wikipedia.org/wiki/Virtual_memory), it uses the `mprotect()` system call to configure the operating system's already-existing virtual memory manager to change the access control flags of regions of process memory.

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

int vk_example_get_val(struct vk_example *example_ptr)
{
	return example_ptr->val;
}
void vk_example_set_val(struct vk_example *example_ptr, int val)
{
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
#include "vk_example.h"
#include "vk_sample.h"
#include "vk_sample_s.h"

int vk_sample_get_example(struct vk_sample *sample_ptr)
{
	return &sample_ptr->example;
}
void vk_sample_set_example(struct vk_sample *sample_ptr, struct vk_example *example_ptr)
{
	vk_example_set_val(&sample_ptr->example, vk_example_get_val(example_ptr));
}
```

### Structured Programming

The intrusive data structure hierarchy allows for structured programming of both data and code. Both data and code can be co-isolated into mini processes, leading to the high cache locality of both data and instruction caches. This enables extremely high vertical scale with easy partitioning for horizontal scale.
