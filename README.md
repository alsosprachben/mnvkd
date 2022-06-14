# M:N Virtual Kernel Daemon

## Synopsis

`mnvkd` is an application server framework for C. Applications are composed of stackless coroutines grouped like micro-threads in sets of micro-processes that each span individual contiguous memory segments that form micro-virtual-memory-spaces. These micro-processes are driven by a network event loop. 

The hierarchy is:
1. stackless coroutine micro-threads in
2. micro-processes in
3. micro-heaps managed by
4. a polling network event loop dispatcher.

The stackless coroutines are provided a blocking I/O interface between OS sockets and other coroutines. Under the hood, the blocking I/O ops are C macros that build state machines that execute I/O futures. The ugly future and blocking logic is hidden behind macros.

### Coroutines

```c
#include "vk_state.h"

void echo(struct that *that) {
	int rc;

	struct {
		size_t i;
		struct {
			char in[8192];
			char out[8192];
		} *buf;
	} *self;

	vk_begin();

	for (self->i = 0; ; self->i++) {
		vk_calloc(self->buf, 1);

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

		vk_free();
	}

	vk_end();
}

#include <fcntl.h>
#include <stdlib.h>
#include "vk_heap.h"
#include "vk_kern.h"
#include "vk_proc.h"

int main(int argc, char *argv[]) {
	int rc;
	struct vk_heap_descriptor *kern_heap_ptr;
	struct vk_kern *kern_ptr;
	struct vk_proc *proc_ptr;
	struct that *vk_ptr;

	kern_heap_ptr = calloc(1, vk_heap_alloc_size());
	kern_ptr = vk_kern_alloc(kern_heap_ptr);
	if (kern_ptr == NULL) {
		return 1;
	}

	proc_ptr = vk_kern_alloc_proc(kern_ptr);
	if (proc_ptr == NULL) {
		return 1;
	}
	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 24);
	if (rc == -1) {
		return 1;
	}

	vk_ptr = vk_proc_alloc_that(proc_ptr);
	if (vk_ptr == NULL) {
		return 1;
	}

	fcntl(0, F_SETFL, O_NONBLOCK);
	fcntl(1, F_SETFL, O_NONBLOCK);

	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 15);
	if (rc == -1) {
		return 1;
	}

	VK_INIT(vk_ptr, proc_ptr, echo, 0, 1);

	vk_proc_enqueue_run(proc_ptr, vk_ptr);
	vk_kern_flush_proc_queues(kern_ptr, proc_ptr);

	rc = vk_kern_loop(kern_ptr);
	if (rc == -1) {
		return -1;
	}

	rc = vk_deinit(vk_ptr);
	if (rc == -1) {
		return 4;
	}

	rc = vk_proc_deinit(proc_ptr);
	if (rc == -1) {
		return 5;
	}

	vk_kern_free_proc(kern_ptr, proc_ptr);

	return 0;
}
```

Coroutine:
  - `vk_state.h`
  - `vk_state_s.h`
  - `vk_state.c`

The stackless coroutines are derived from [Simon Tatham's coroutines](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html) based on [Duff's Device](https://en.wikipedia.org/wiki/Duff%27s_device) to build stackless state-machine coroutines. But the coroutines in `mnvkd` have a novel enhancement: instead of using the `__LINE__` macro twice for each yield (firstly for setting the return stage, and secondly for the `case` statement to jump back to that stage), the `__COUNTER__` and `__COUNTER__ - 1` macros are used. This makes it possible to wrap multiple yields in higher-level macros. When using `__LINE__`, an attempt to have one macro call multiple yields will have all yields collapse onto a single line, so the multiple stages will have the same state number, resulting in multiple case statements with the same value, a syntax error. This problem is resolved by using `__COUNTER__`. Although `__COUNTER__` is not standard C, virtually every compiler has this feature (and it is trivially implementable in a custom preprocessor), and this allows high-level blocking operations to be built on lower-level blocking operations. The result is a thread-like interface, but with the very low overhead of state machines and futures.

The coroutine state is accessible via `that`, and the state-machine state-variable is an anonymous struct `self` declared at the top of the coroutine. These coroutines are stackless, meaning that stack variables may be lost between each blocking op, so any state-machine state must be preserved in memory associated with the coroutine, not the stack.

### Exceptions

Errors can yield via `vk_raise(error)` or `vk_error()`, but instead of yielding back to the same execution point, they yield to a `vk_finally()` label. A coroutine can only have a single finally label for all cleanup code, but the cleanup code can branch and yield `vk_lower()` to lower back to where the error was raised. High-level blocking operations raise errors automatically. 

The nested yields provide a very simple way to build a zero-overhead framework idiomatic of higher-level C-like languages, but with the simplicity and power of pure C. Nothing gets in the way of pure C. 

### Sockets

Socket:
  - `vk_socket.h`
  - `vk_socket_s.h`
  - `vk_socket.c`

Pipe:
  - `vk_pipe.h`
  - `vk_pipe_s.h`
  - `vk_pipe.c`

Each coroutine may have a default socket object that represents its Standard I/O. This socket may be bound to physical file descriptors, or be logically entangled with other sockets, as inter-coroutine message passing channels. These bindings are represented as a Pipe that holds either a file descriptor or a pointer to a socket's receive or send queue. The same blocking ops that work against file descriptors also work across these inter-coroutine channels, in which case data is simply spliced to and from socket queues.

Alternatively, a coroutine may yield a future directly to another coroutine, passing a reference directly to memory, rather than buffering data in a socket queue. This is more in the nature of a future.

A parent coroutine can spawn a child coroutine, where the child inherits the parent's socket. Otherwise, a special "pipeline" child can be used, which, on start, creates pipes to the parent like a unix pipeline. That is, the parent's standard output is given to the child, and the parent's standard output is instead piped to the child's standard input.  A normal child uses `vk_begin()` just like a normal coroutine, and a pipeline child uses `vk_begin_pipeline(future)`, which sets up the pipeline to the parent, and receives an initialization future message. All coroutines end with `vk_end()`, which is required to end the state machine's switch statement, and to free `self` and the default socket allocated by `vk_begin()`.

API:
 - `vk_read()`: read a fixed number of bytes into a buffer, or until EOF
 - `vk_readline()`: read a fixed number of bytes into a buffer, or until EOF or a newline
 - `vk_write()`: write a fixed number of bytes from a buffer
 - `vk_flush()`: block until everything written is has been physically sent
 - `vk_eof()`: EOF has been reached
 - `vk_clear()`: clear EOF status
 - `vk_nodata()`: EOF has been reached, and all data has been received
 - `vk_hup()`: hang up writing -- EOF on receiving side
 - `vk_hanged()`: hang up status
 - `vk_read_splice()`: read into socket has been sent into other socket
 - `vk_write_splice()`: write into socket what has been received into other socket

For each `vk_*()` op, there is an equal `vk_socket_*()` op that operates on a specified socket, rather than the coroutine default socket. 

### Vectorings: I/O Vector Ring Buffers

Vectoring:
  - `vk_vectoring.h`
  - `vk_vectoring_s.h`
  - `vk_vectoring.c`

The underlying OS socket operations send and receive between "I/O Vectors" called `struct iovec`, each an array of memory segments. A ring buffer can be represented by 1 or 2 I/O vectors. A vectoring object holds the ring buffer, and maintains a consistent view of the ring buffer via I/O vector pairs representing each of the parts for transmitting and receiving. This provides an intrinsic I/O queue suitable for the micro-heap.

### Micro-Heaps of Garbage-Free Memory

Micro-Heap:
  - `vk_heap.h`
  - `vk_heap_s.h`
  - `vk_heap.c`

A set of coroutines are grouped into a contigious memory mapping, a micro-heap. Coroutines within the micro-process pass execution around within a single dispatch, only involving the memory in the micro-heap. When the micro-process is not running, its micro-heap has read access disabled until it restarts. 

Instead of using externally-linked containers, the system intrinsic lists, `#include <sys/queue.h>`, are rather used. As intrisic lists, the container attributes are embedded directly into the elements. This requires being explicit about which set memberships an element may have, but keeps memory local and unfragmented. 

Memory is allocated from the heap as a stack of pages, allocated and deallocated hierarchically, in the paradigm of structured programming, in stack order (first allocated, last deallocated). The memory lifecycle is much more suited to a stack than execution. An object has one life, but may easily be executed in cycles. Loops tend to be stack-ordered, so loops that reallocate objects can do so with no overhead nor fragmentation.

In fact, the generational aspect of memory acknowledged by modern generational gargage collection technique is a reflection of this stack-based order of memory allocation. So instead of using garbage collection, a process-oriented stack of memory is all that is needed. Generally, when memory lifecycles are not stack-ordered, there is concurrency, and each concurrent process should then get its own micro-process and stack-oriented memory heap.

### Micro-Processes and Infra-Process Futures

Micro-Process:
  - `vk_proc.h`
  - `vk_proc_s.h`
  - `vk_proc.c`

Infra-Process Future:
  - `vk_future.h`
  - `vk_future_s.h`
  - `vk_future.c`

Run and blocking queues are per-heap, forming a micro-process that executes until the run queue is drained, leaving zero or more coroutines in the blocking queue. That is, a single dispatch progresses the execution in the micro-process as far as possible, via execution intra-futures (internal to the process), then blocks. Each coroutine's execution status is one of the following:
1. currently executing,
2. in the micro-process run queue,
3. held as an intra-process future in a coroutine's state (another form of queue), or
4. held as an inter-process I/O future in the network poller's state (another form of queue).

### Micro-Poller and Inter-Process Futures

Inter-Process Future:
  - `vk_poll.h`
  - `vk_poll_s.h`
  - `vk_poll.c`

Each I/O block forms an inter-process I/O future (external to the process). When a poller has detected that the block is invalid, and the coroutine may continue, the Inter-Process future allows it to reschedule the coroutine, re-enable access to its micro-process's micro-heap, then restart the micro-process.

The network poller is therefore a privileged process that is effectively the kernel. There is [a proposed design for system calls to protect this privileged kernel memory](https://spatiotemporal.io/#proposalasyscallisolatingsyscallforisolateduserlandscheduling), forming a new type of isolating virtualization.

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

