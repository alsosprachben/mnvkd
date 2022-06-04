# M:N Virtual Kernel Daemon

## Synopsis

`mnvkd` is an application server framework for C. Applications are composed of micro-heap memory spaces of stackless coroutines with a blocking I/O interface between OS sockets and other coroutines. Under the hood, the blocking I/O ops are C macros that build state machines that execute I/O futures. The ugly future and blocking logic is hidden behind macros.

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
#include "vk_proc.h"

int main(int argc, char *argv[]) {
	int rc;
	struct vk_proc *proc_ptr;
	struct that *vk_ptr;

	proc_ptr = calloc(1, vk_proc_alloc_size());
	vk_ptr = calloc(1, vk_alloc_size());

	fcntl(0, F_SETFL, O_NONBLOCK);
	fcntl(1, F_SETFL, O_NONBLOCK);

	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 15);
	if (rc == -1) {
		return 1;
	}

	VK_INIT(vk_ptr, proc_ptr, echo, 0, 1);

	vk_proc_enqueue_run(proc_ptr, vk_ptr);
	do {
		rc = vk_proc_execute(proc_ptr);
		if (rc == -1) {
			return 2;
		}
		rc = vk_proc_poll(proc_ptr);
		if (rc == -1) {
			return 3;
		}
	} while (vk_proc_pending(proc_ptr));

	rc = vk_deinit(vk_ptr);
	if (rc == -1) {
		return 4;
	}

	rc = vk_proc_deinit(proc_ptr);
	if (rc == -1) {
		return 5;
	}

	return 0;
}
```

Coroutine:
  - `vk_state.h`
  - `vk_state_s.h`
  - `vk_state.c`

The stackless coroutines are derived from [Simon Tatham's coroutines](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html), but with a novel enhancement. Instead of using the `__LINE__` macro twice, the `__COUNTER__` and `__COUNTER__ - 1` macros are used. This makes it possible to nest yields. This nesting means that very high-level blocking operations can be built on lower-level blocking operations. The result is a thread-like interface, but with the very low overhead of state machines and futures.

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

### Micro-Processes

Micro-Process:
  - `vk_proc.h`
  - `vk_proc_s.h`
  - `vk_proc.c`

Micro-Heap:
  - `vk_heap.h`
  - `vk_heap_s.h`
  - `vk_heap.c`

Process-Intra-Future:
  - `vk_future.h`
  - `vk_future_s.h`
  - `vk_future.c`

Process-Inter-Future:
  - `vk_poll.h`
  - `vk_poll_s.h`
  - `vk_poll.c`

A set of coroutines are grouped into a contigious memory mapping, a micro-heap. Run and blocking queues are per-heap, forming a micro-process that executes until the run queue is drained, leaving zero or more coroutines in the blocking queue. That is, a single dispatch progresses the execution in the micro-process as far as possible, via execution intra-futures (internal to the process), then blocks, where each block forms an I/O inter-future (external to the process).

Coroutines within the micro-process pass execution around within a single dispatch, only involving the memory in the micro-heap. When the micro-process is not running, its micro-heap has read access disabled. To continue execution, a network poller dispatches the inter-futures back to their micro-processes, re-enabling read access by reference from the I/O inter-future object.

The network poller is therefore a privileged process that is effectively the kernel. There is [a proposed design for system calls to protect this privileged kernel memory](https://spatiotemporal.io/#proposalasyscallisolatingsyscallforisolateduserlandscheduling), forming a new type of isolating virtualization.

### Vectorings: I/O Vector Ring Buffers

Vectoring:
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

