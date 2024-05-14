# `mnvkd` Object Interfaces

## Threading Object Interfaces

### Coroutines

`struct vk_thread`: Coroutine thread
- [`vk_thread.h`](vk_thread.h)
- [`vk_thread_s.h`](vk_thread_s.h)
- [`vk_thread.c`](vk_thread.c)


### Micro-Process Threading Runtime

`struct vk_proc_local`: Local Micro-Process (inside micro-heap)
- [`vk_proc_local.h`](vk_proc_local.h)
- [`vk_proc_local_s.h`](vk_proc_local_s.h)
- [`vk_proc_local.c`](vk_proc_local.c)

#### Meta Information

##### Values
- `proc_id`: Process ID, used as a foreign key to the kernel process table.

##### Methods
- `vk_proc_local_get_proc_id(proc_local_ptr)`: Get the process ID.
- `vk_proc_local_set_proc_id(proc_local_ptr, proc_id)`: Set the process ID.
- `vk_proc_local_init(proc_local_ptr)`: Reinitialize queues.
- `vk_proc_local_alloc_size()`: `sizeof (struct vk_proc_local)`

#### Execution Queues

##### Values
- `run`: Boolean for whether the run queue is not empty.
- `block`: Boolean for whether in the blocked queue is not empty.
- `run_q_head`: The `queue.h` list head for the run queue of coroutines within this micro-process.
- `block_q_head`: The `queue.h` list head for the blocked queue of coroutines within this micro-process.

##### Methods for Virtual Kernel
- `vk_proc_local_get_run(proc_local_ptr)`: Get the run queue status. (for foreign inspection)
- `vk_proc_local_get_block(proc_local_ptr)`: Get the blocked queue status. (for foreign inspection)
- `vk_proc_local_is_zombie(proc_local_ptr)`: Both queues are empty, and the process is ready to be deallocated.
- `vk_proc_local_retry_socket(proc_local_ptr, socket_ptr)`: Poller observed readiness to proceed with the specified socket. This retries the socket's blocked operation to make more progress.
- `vk_proc_local_execute(proc_local_ptr)`: Execute the micro-process, dispatching coroutines from the run queue until it is empty, and only blocked sockets remain.

##### Private Methods
- `vk_proc_local_set_run(proc_local_ptr, run)`: Set the run queue status. (used internally)
- `vk_proc_local_set_block(proc_local_ptr, block)`: Set the blocked queue status. (used internally)
- `vk_proc_local_enqueue_run(proc_local_ptr, that)`: Enqueue a coroutine to the run queue.
- `vk_proc_local_dequeue_run(proc_local_ptr)`: Dequeue a coroutine from the run queue.
- `vk_proc_local_drop_run(proc_local_ptr, that)`: Drop a coroutine from the run queue.
- `vk_proc_local_enqueue_blocked(proc_local_ptr, socket_ptr)`: Enqueue a socket to the blocked queue.
- `vk_proc_local_dequeue_blocked(proc_local_ptr)`: Dequeue a socket from the blocked queue.
- `vk_proc_local_drop_blocked(proc_local_ptr, socket_ptr)`: Drop a socket from the blocked queue.
- `vk_proc_local_drop_blocked_for(proc_local_ptr, that)`: Drop sockets from blocked queue referenced by coroutine.

#### Signal Handling

##### Values
- `running_cr`: The currently executing coroutine, the target for signals, if `supervisor_cr` is not set.
- `supervisor_cr`: The coroutine that supervises the micro-process, the target for signals, if set.
- `siginfo`: The signal information, if a signal is pending.
- `uc_ptr`: The `ucontext_t` at the generated signal.

##### Public Methods for Virtual Kernel
- `vk_proc_local_set_siginfo(proc_local_ptr, siginfo)`: Set `siginfo_t siginfo` from raised signal. 
- `vk_proc_local_set_uc(proc_local_ptr, uc_ptr)`: Set `ucontext_t *uc_ptr` from raised signal.
- `vk_proc_local_raise_signal(proc_local_ptr)`: Raises EFAULT to either the currently running coroutine, or if set, the supervisor coroutine. The signal information should also be set outside of this function.

##### Private Methods
- `vk_proc_local_get_siginfo(proc_local_ptr)`: Get `siginfo_t` raised from signal.
- `vk_proc_local_get_uc(proc_local_ptr)`: Get `ucontext_t` from raised signal.
- `vk_proc_local_get_supervisor(proc_local_ptr)`: Get supervisor coroutine.
- `vk_proc_local_set_supervisor(proc_local_ptr, that)`: Set supervisor coroutine.


#### Memory

##### Values
- `stack`: The stack of pages allocated for the micro-process.

##### Private Methods
- `vk_proc_local_get_stack(proc_local_ptr)`: Get stack object spanning the micro-heap.
- `vk_proc_local_set_stack(proc_local_ptr, stack)`: Copy stack struct into local process.

### Micro-Heaps of Garbage-Free Memory

`struct vk_stack`: Micro-Stack
- [`vk_stack.h`](vk_stack.h)
- [`vk_stack_s.h`](vk_stack_s.h)
- [`vk_stack.c`](vk_stack.c)

`struct vk_heap`: Micro-Heap
- [`vk_heap.h`](vk_heap.h)
- [`vk_heap_s.h`](vk_heap_s.h)
- [`vk_heap.c`](vk_heap.c)

A set of coroutines are grouped into a contiguous memory mapping, a micro-heap. Coroutines within the micro-process pass execution around within a single dispatch, only involving the memory in the micro-heap. When the micro-process is not running, its micro-heap has read access disabled until it restarts.

Instead of using externally-linked containers, the system intrinsic lists, `#include <sys/queue.h>`, are rather used. As intrinsic lists, the container attributes are embedded directly into the elements. This requires being explicit about which set memberships an element may have, but keeps memory local and un-fragmented.

Memory is allocated from the heap as a stack of pages, allocated and de-allocated hierarchically, in the paradigm of structured programming, in stack order (first allocated, last de-allocated). The memory lifecycle is much more suited to a stack than execution. An object has one life, but may easily be executed in cycles. Loops tend to be stack-ordered, so loops that reallocate objects can do so with no overhead nor fragmentation.

In fact, the generational aspect of memory acknowledged by modern generational garbage collection technique is a reflection of this stack-based order of memory allocation. So instead of using garbage collection, a process-oriented stack of memory is all that is needed. Generally, when memory life-cycles are not stack-ordered, there is concurrency, and each concurrent process should then get its own micro-process and stack-oriented memory heap.

### Vectorings: I/O Vector Ring Buffers

`struct vk_vectoring`: Vector Ring
- [`vk_vectoring.h`](vk_vectoring.h)
- [`vk_vectoring_s.h`](vk_vectoring_s.h)
- [`vk_vectoring.c`](vk_vectoring.c)

The underlying OS socket operations send and receive between "I/O Vectors" called `struct iovec`, each an array of memory segments. A ring buffer can be represented by 1 or 2 I/O vectors. A vectoring object holds the ring buffer, and maintains a consistent view of the ring buffer via I/O vector pairs representing each of the parts for transmitting and receiving. This provides an intrinsic I/O queue suitable for the micro-heap.

## Virtual Kernel Object Interfaces

### Micro-Processes and Intra-Process Futures

`struct vk_proc`: Global Micro-Process (outside micro-heap)
- [`vk_proc.h`](vk_proc.h)
- [`vk_proc_s.h`](vk_proc_s.h)
- [`vk_proc.c`](vk_proc.c)


`struct vk_future`: Intra-Process Future
- [`vk_future.h`](vk_future.h)
- [`vk_future_s.h`](vk_future_s.h)
- [`vk_future.c`](vk_future.c)

Run and blocking queues are per-heap, forming a micro-process that executes until the run queue is drained, leaving zero or more coroutines in the blocking queue. That is, a single dispatch progresses the execution in the micro-process as far as possible, via execution intra-futures (internal to the process), then blocks. Each coroutine's execution status is one of the following:
1. currently executing,
2. in the micro-process run queue,
3. held as an intra-process future in a coroutine's state (another form of queue), or
4. held as an inter-process I/O future in the network poller's state (another form of queue).

A coroutine may yield a future directly to another coroutine, passing a reference directly to memory, rather than buffering data in a socket queue.

A parent coroutine can spawn a child coroutine, where the child inherits the parent's socket. Otherwise, a special "pipeline" child can be used, which, on start, creates pipes to the parent like a unix pipeline. That is, the parent's standard output is given to the child, and the parent's standard output is instead piped to the child's standard input.  A normal child uses `vk_begin()` just like a normal coroutine, and a pipeline child uses `vk_begin_pipeline(future)`, which sets up the pipeline to the parent, and receives an initialization future message. All coroutines end with `vk_end()`, which is required to end the state machine's switch statement, and to free `self` and the default socket allocated by `vk_begin()`.

### Micro-Poller and Inter-Process Futures

`struct vk_io_future`: Inter-Process Future
- [`vk_io_future.h`](vk_io_future.h)
- [`vk_io_future_s.h`](vk_io_future_s.h)
- [`vk_io_future.c`](vk_io_future.c)

Each I/O block forms an inter-process I/O future (external to the process). When a poller has detected that the block is invalid, and the coroutine may continue, the Inter-Process future allows it to reschedule the coroutine, re-enable access to its micro-process's micro-heap, then restart the micro-process.

### Kernel Network Event Loop

`struct vk_kern`: Userland Kernel
- [`vk_kern.h`](vk_kern.h)
- [`vk_kern_s.h`](vk_kern_s.h)
- [`vk_kern.c`](vk_kern.c)

The network poller is therefore a privileged process that is effectively the kernel that dispatches processes from the run queue and the ready events from the blocked queue. There is [a proposed design for system calls to protect this privileged kernel memory](https://spatiotemporal.io/#proposalasyscallisolatingsyscallforisolateduserlandscheduling), forming a new type of isolating virtualization.

The kernel is allocated from its own micro-heap of contiguous memory that holds:
1. the file descriptor table,
2. the process table,
3. the heads of the run and blocked queues.

To improve isolation, when a process is executing, its change in membership of the kernel run queue and blocked queue is held locally in `struct vk_proc_local` until after execution, then the state is flushed outside execution scope to `struct vk_proc` members with head in `struct vk_kern`. Manipulating the queues involves manipulating list links on other processes, and only the kernel can manipulate other processes, so the linked-list is global, while mere boolean flags are held locally. This way, the micro-heaps only contain local references.

### File Descriptor Table

`struct vk_fd_table`: FD Table
- [`vk_fd_table.h`](vk_fd_table.h)
- [`vk_fd_table_s.h`](vk_fd_table_s.h)
- [`vk_fd_table.c`](vk_fd_table.c)

`struct vk_fd`: FD network poller state
- [`vk_fd.h`](vk_fd.h)
- [`vk_fd_s.h`](vk_fd_s.h)
- [`vk_fd.c`](vk_fd.c)

The file descriptor table holds the network event registration state, and I/O between the network poller, either `poll()`. `epoll()`, `io_submit()`, or `kqueue()`. The FDs can be flagged as:
- dirty: A new network block exists, so state needs to be registered with the poller.
- fresh: Events were returned by the poller, so the blocked process needs to continue.

The polling time lifecycle state is kept:
1. pre: needs to be registered with the poller, "dirty" flushing to:
2. post: currently registered with the poller, "fresh" flushing to:
3. ret: dispatch to the process, to continue the blocked virtual thread.

A network poller device simply registers the dirty FDs, and returns fresh FDs.
