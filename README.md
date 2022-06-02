# M:N Virtual Kernel Daemon

## Intro

`mnvkd` is an application server framework for C. Applications are composed of micro-heap memory spaces of stackless coroutines with a blocking I/O interface between OS sockets and other coroutines. Under the hood, the blocking I/O ops are C macros that build state machines that execute I/O futures. The ugly future and blocking logic is hidden behind macros.

### Coroutines

Coroutines:
  - `vk_state.h`
  - `vk_state_s.h`
  - `vk_state.c`

  - `vk_socket.h`
  - `vk_socket_s.h`
  - `vk_socket.c`

The stackless coroutines are derived from [Simon Tatham's coroutines](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html), but with a special enhancement. Instead of using the `__LINE__` macro twice, the `__COUNTER__` and `__COUNTER__ - 1` macros are used. This makes it possible to nest yields. This nesting means that very high-level blocking operations can be built on lower-level blocking operations. The result is a thread-like interface, but with the very low overhead of state machines and futures.

The coroutine state is accessible via `that`, and the state-machine state-variable is an anonymous struct `self` declared at the top of the coroutine. These coroutines are stackless, meaning that stack variables are lost between each blocking op, so any state-machine state must be preserved in memory associated with the coroutine, not the stack.

Each coroutine may have a default socket object that represents its Standard I/O. This socket may be bound to physical file descriptors, or be logically entangled with other sockets, as inter-coroutine message passing channels. The same blockking ops that work against file descriptors also work across these inter-coroutine channels. 

### Micro-Processes

Micro-Process:
  - `vk_proc.h`
  - `vk_proc_s.h`
  - `vk_proc.c`

Micro-Heap:
  - `vk_heap.h`
  - `vk_heap_s.h`
  - `vk_heap.c`

Intra-Futures:
  - `vk_future.h`
  - `vk_future_s.h`
  - `vk_future.c`

Inter-Futures:
  - `vk_poll.h`
  - `vk_poll_s.h`
  - `vk_poll.c`

A set of coroutines are grouped into a contigious memory mapping, a micro-heap. Run and blocking queues are per-heap, forming a micro-process that executes until the run queue is drained, leaving coroutines in the blocking queue. That is, a single dispatch progresses the execution in the micro-process as far as possible, via execution intra-futures (internal to the process), then blocks, where each block forms an I/O inter-future (external to the process). Coroutines within the micro-process pass execution around within a single dispatch, only involving the memory in the micro-heap. When the micro-process is not running, its micro-heap has read access disabled. A network poller dispatches the inter-futures back to their micro-processes.

### Vectorings: I/O Vector Ring Buffers

Vectorings:
  - `vk_vectoring.h`
  - `vk_vectoring_s.h`
  - `vk_vectoring.c`

The underlying OS socket operations send and receive between "I/O Vectors" called `struct iovec`, each an array of memory segments. A ring buffer can be represented by 1 or 2 I/O vectors. A vectoring object holds the ring buffer, and maintains a consistent view of the ring buffer via I/O vector pairs representing each of the parts for transmitting and receiving. This provides an intrinsic I/O queue suitable for the micro-heap. 

## Design

### Horizontal Layers of Abstraction
To reduce the size of computational problems, software frameworks are often comprised of horizontal layers of abstraction. Data passes vertically through each layer, each layer processing all data, but at different stages of the process. Therefore, a "software stack" implements a "data pipeline". The use of a pipeline increases throughput, but the complexity of using many layers also increases latency.

### Mapping and Reducing: Big Data as Small Data
When "big data" is involved, a single big data process becomes comprised of many big data processes, one for each layer. The way to deal with a big data problem is to make it a small data problem. To do this, a "distributed system" is developed, where each layer is partitioned, data is "mapped" to each partition, then "reduced" on the other side. This terminology comes from the `map()` and `reduce()` functions common in the domain of functional programming. Sometimes this is called splitting and merging, or scattering and gathering. 

### Vertical Layers of Abstraction
Ideally, data should be mapped once at the beginning, then reduced once at the end, in contiguous vertical layers of abstraction, but often distributed systems end up unnecessarily re-reducing and re-mapping in between each layer. Such an architecture is often a symptom of a distributed system that unfortunately inherited the design of a non-distributed system that started out with horizontal layers of abstraction. Therefore, it can be beneficial for a distributed system to be designed from the beginning with vertical layers of abstraction. 

### Locality of Reference
With proper vertical layer partitioning, the horizontal layers can become local again. This completely changes the algorithmic conditions. The need for slow and costly cloud methods disappears:
  - No need for complex distributed hash tables just to store variables.
  - No need for complex service meshes to manage horizontal connections beween systems.
  - No need for complex data lakes with complex batch processing latencies. 
  - No need for complex inter-lake journals with complex data pipeline management. 
  - No need for complex function dispatching "serverless" systems. 

Applications are small and efficient again, but exist within a modern horizontal resource backplane supporting local access, and this "locality of reference" mitigates the latency problem of distributed data pipelines.

### Platforms Manage Resources
In a computing system, the operating system's task is to manage the physical resources. The task of a platform's standard library is to interface with the operating system to expose the physical resources to design patterns, by implementing common algorithms optimized for resource management.

For example, the standard library implements various object containers, like sets, tables, lists, and queues, and provides interfaces for performing input and output with those containers. This explains why container implementations differ greatly across different platforms: the differences of resource management opinions. For example:
 - A platform that has garbage collection has containers with external references, often mixing object types in a single container, becuase the type information is associated with the object storage.
 - However, a platform with manually managed memory often uses internal references, and a container only supporting a single type, because the container is comprised of element references, and a fixed-sized head object.
 - A platform with stack-based threads needs a thread-aware memory allocator to deal with memory transactions.
 - However, an event-based platform with futures shares all memory, and all memory transactions are within a single thread.

### Frameworks Invert Control
In a typical functional computing environment, dependent platform code is linked as external functions called from application functions. The application is completely in control over its dependencies, and needs configuration changes to use different dependencies.

Rather than acting as a dependent library, a framework can invert control so that rather than being called by applications, the framework instead calls into applications, and the applications have no direct control over the framework. This simplifies the interface with the application, and makes it easier to unify configuration across applications.

