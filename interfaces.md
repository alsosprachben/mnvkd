# M:N Virtual Kernel Daemon Interfaces

## Coroutines

### Stackless Coroutine Micro-Threads
`struct vk_thread`:
- [`vk_thread.h`](vk_thread.h): public interface
- [`vk_thread_s.h`](vk_thread_s.h): private struct
- [`vk_thread.c`](vk_thread.c): implementation

#### Blocking Macros
- [`vk_thread_cr.h`](vk_thread_cr.h): [Coroutine API](#coroutine-api)
- [`vk_thread_mem.h`](vk_thread_mem.h): [Memory API](#memory-api)
- [`vk_thread_exec.h`](vk_thread_exec.h): [Execution API](#execution-api)
- [`vk_thread_ft.h`](vk_thread_ft.h): [Future API](#future-api)
- [`vk_thread_err.h`](vk_thread_err.h): [Exception API](#exception-api)
- [`vk_thread_io.h`](vk_thread_io.h): [Socket API](#socket-api)

#### Technology
The stackless coroutines are derived from [Simon Tatham's coroutines](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html) based on [Duff's Device](https://en.wikipedia.org/wiki/Duff%27s_device) to build stackless state-machine coroutines. But the coroutines in `mnvkd` have a novel enhancement: instead of using the `__LINE__` macro twice for each yield (firstly for setting the return stage, and secondly for the `case` statement to jump back to that stage), the `__COUNTER__` and `__COUNTER__ - 1` macros are used. This makes it possible to wrap multiple yields in higher-level macros. When using `__LINE__`, an attempt to have one macro call multiple yields will have all yields collapse onto a single line, so the multiple stages will have the same state number, resulting in multiple case statements with the same value, a syntax error.

This problem is resolved by using `__COUNTER__`. Although `__COUNTER__` is not standard C, virtually every compiler has this feature (and it is trivially implementable in a custom preprocessor), and this allows high-level blocking operations to be built on lower-level blocking operations. The result is a thread-like interface, but with the very low overhead of state machines and futures.

The coroutine state is accessible via:
1. `struct vk_thread *that`, the only argument to a coroutine function, and
2. `struct { /* local state declarations */ } *self`, an anonymous struct declared at the top of the coroutine, before the `vk_begin()`. The `*self` value is allocated from the micro-heap by `vk_begin()`, and ultimately freed by `vk_end()`.

These coroutines are stackless, meaning that stack variables may be lost between each blocking op (which will *always* start with `vk_`), so any state-machine state spanning a blocking op must be preserved in memory associated with the coroutine (`*that` or `*self`), not the C stack locals. However, in between `vk_*()` macro invocations, the C stack locals behave normally, *but assume they are re-uninitialized by a coroutine restart*.

### Paradigm Mitigation

 Paradigms have a tendency to need to fold in on themselves, to counter their polarity. This is a general system design principle that is applicable to any complex system. Extreme ideologies tend to have sub-ideologies to mitigate their incompleteness. This can be called "*Paradigm Mitigation*".

#### Functional Threads, Procedural Futures
Normally, threads are considered procedural, and futures (and events) are considered functional. However, In this case, threads can be made more functional, and futures can be made more procedural. In this way, the highly opinionated platform logic of an actor-based messaging system can be brought to a completely unfettered, un-opinionated C environment. This is in the same spirit as "[easy things should be easy, complex things should be possible](https://www.quora.com/What-is-the-story-behind-Alan-Kay-s-adage-Simple-things-should-be-simple-complex-things-should-be-possible/answer/Alan-Kay-11)", a sentiment commonly re-quoted by platform authors (in Perl and Python communities, for example).

#### Syntactic Sugar
The coroutines are similar to an `async` and `await` language syntax that wraps futures in "syntactic sugar" that hides the low-level handling of the futures. This is a great example of paradigm mitigation.

However, unlike regular futures, a blocking operation in `mnvkd` is not entirely implemented by another function. In fact, the current function can sometimes implement the entire operation on its own. That is because of a fundamental distinction between asynchronous operations and blocking operations.

#### Asynchronous Operation versus Synchronous Operation

##### Asynchronous Operation
An "async" operation is an operation that awaits "<em>completion</em>". That is, an operation is "<em>submitted</em>" to another subsystem, then <em>that</em> subsystem <em>notifies of completion</em>. The completion is a <em>status change</em> that can be <em>checked</em> or <em>polled</em>, or even <em>cancelled</em>.

##### Synchronous Operation
A "synchronous" operation is an operation that is "<em>structured</em>". That is, an operation is called then returns. There is no <em>interface</em> for what happens during the call. Indeed, the distinction is only one of <em>interface</em>.

#### Blocking Operation versus Non-Blocking Operation

##### Blocking Operation
A "blocking" operation is a synchronous operation, where an operation is called and completed entirely for the return.

##### Non-Blocking Operation
A "non-blocking" operation <em>not</em> an <em>asynchronous</em> operation. In fact, it is still a <em>synchronous</em> operation. It merely returns immediately, performing whatever part of the operation was <em>ready</em> to be completed <em>at the time</em>.

#### Blocking Operation <em>over</em> Non-blocking Operation
What is done <em>asynchronously</em> around non-blocking operations is actually the <em>polling for readiness</em> to perform <em>more</em> of the <em>blocking</em> operation. That is, what is notified is not <em>completion</em>, but rather <em>an underlying resource status change</em> that allows <em>more</em> of the operation to proceed.

##### Event-Based Programming
That is, what is enqueued are not <em>operations</em>, but rather <em>events</em>. That is why <em>non-blocking programming</em> is also called <em>event-based programming</em>.

When disk I/O is performed, a kernel process performs the operation atomically against a single resource, then notifies of completion.

However, When network I/O is performed, a kernel process performs or processes packets against a single resource, but related to a socket shared with the user process, and the <em>underlying readiness</em> is triggered by packets <em>sent to and acknowledge by</em> (`POLLOUT`) or <em>received by</em> (`POLLIN`) a <em>remote</em> peer, which has its own associated user process socket.

There typically needs to be a loop in userland over each operation, until the operation is complete. Each iteration of the loop needs to <em>poll for readiness</em> of each non-blocking operation. This is called an "<em>event loop</em>".

##### Threaded Programming
Threads and processes can implement blocking operations, but they do this by implementing an internal event loop, and control the scheduling, execution, and continuation of blocked tasks.

That is, what threads _are_ is the _logical_ mapping of event-based (unstructured), asynchronous, functional programming as structured, synchronous, procedural programming.

| system    | return       | scheduling   | ontological |
|-----------|--------------|--------------|-------------|
| *events*  | unstructured | asynchronous | functional  |
| *threads* | structured   | synchronous  | procedural  |

What `await` does is this kind of mapping, to be more thread-like. The `await` syntax sugar makes the interface synchronous, with a structured return in a functional procedure. However, it pushes blocking logic to the next call frame. Therefore, the procedural nature is more syntactical.

Alternatively, as in `mnvkd` coroutines, the blocking logic can be kept to the blocking operation's call frame, implemented by a state machine compiled at build-time, where future logic (I/O future) is only used when the high-level operation is actually blocked by an immediate lack of resources.

In `mnvkd`, I/O futures (`struct vk_io_future`) are different from local futures (`struct vk_future`), because they carry isometric blocking state between the micro-processes and the virtual kernel's poller. That is, the high-level, local blocking operations get all of the poll state.

This state-oriented message-passing of polling state enables a higher _procedural_ nature of thread. That is, rather than nested _functions_, the futures are rolling in a loop that _flattens the stack_, wrapped in _blocking conditions_ that make them lazy.

This is actually _more_ functional, because the whole procedure is built by macros into a single _state machine_ function. The reason why this is _more_ functional is that the functions are inlined, allowing the compiler to perform lambda calculus on the nested operations, and produce a single function that inlines all blocks.

That is, each thread function's code can be viewed as a single process's code. So not only is data cache highly structured by the micro-heaps, the instruction cache is also much more structured than with vanilla futures with scattered code.

In fact, the *entire runtime* for coroutine threads is actually just a single function that simply iterates over the run queue: `vk_proc_local.c:vk_proc_local_execute()`. The complexity is rather in the micro-processes and their relationships to resource polling.

### Coroutine API
[`vk_thread_cr.h`](vk_thread_cr.h):
- `vk_begin()`: start stackless coroutine state machine
- `vk_end()`:  end stackless coroutine state machine
- `vk_yield(s)`: yield stackless coroutine state machine, where execution exits and re-enters

The `vk_begin()` and `vk_end()` wrap the lifecycle of:
1. The state machine `switch (vk_get_counter(that))`.
2. The allocation of the `self` anonymous struct.
3. The allocation of the `socket_ptr` default socket.

The `vk_yield()` builds the `vk_set_counter(that, __COUNTER__);`, `case __COUNTER__ - 1:;` to implement the yield out of the coroutine. It also marks the `__LINE__` for debugging, and sets the `enum VK_PROC_STAT` process status, a parameter to the yield.

The `s` argument to `vk_yield()` is `enum VK_PROC_STAT` defined in `vk_thread.h`. The values are for the most primitive execution loop logic:
```c
enum VK_PROC_STAT {
	VK_PROC_RUN = 0, /* This coroutine needs to run at the start of its run queue. */
	VK_PROC_YIELD,	 /* This coroutine needs to run at the end   of its run queue. */
	VK_PROC_LISTEN,	 /* This coroutine is waiting for a vk_request(). */
	VK_PROC_WAIT,	 /* This coroutine is waiting for I/O. */
	VK_PROC_ERR,	 /* This coroutine needs to run, jumping to the vk_finally(). */
	VK_PROC_END,	 /* This coroutine has ended. */
};
```

#### Minimal Example
From [`vk_test_cr.c`](vk_test_cr.c):
```c
#include <stdio.h>

#include "vk_main_local.h"

void example(struct vk_thread *that)
{
	struct {
		int i;
	} *self;
	vk_begin();
	for (self->i = 0; self->i < 10; self->i++) {
		dprintf(1, "iteration %i\n", self->i);
		vk_yield(VK_PROC_YIELD); /* low-level of vk_pause() */
	}
	vk_end();
}

int main() {
	return vk_local_main_init(example, NULL, 0, 26);
}
```

The `VK_PROC_YIELD` state tells the execution loop to place the thread back in `VK_PROC_RUN` state, ready to be enqueued in the run queue. This is actually how `vk_pause()` is implemented, which is part of how `vk_call()` is implemented. This demonstrates how being able to nest with `__COUNTER__` enables a clean, higher-level interface that is more thread-like, and easy to extend.

##### Local Coroutine Executor
`vk_main_local.h`:
- `vk_local_main_init(main_vk, buf, buflen, page_count)`
  - `main_vk`: The coroutine function.
  - `buf`: pointer to buffer to copy into the beginning of `self`, as an argument to the coroutine.
  - `buflen`: the number of bytes to copy.
  - `page_count`: the number of 4096 byte pages to allocate for the coroutine's micro-heap.

This interface uses only the micro-process local interfaces, which run in the micro-heap. No virtual kernel is running, so no polling nor inter-micro-process scheduling is done. This is mainly for testing. It binds `stdin` and `stdout` to the coroutine's default socket, in blocking mode, since there is no polling. It runs the coroutine once, until it yields or blocks.

This provides a stateful thread library runtime as a single function that iterates over the run queue: `vk_proc_local.c:vk_proc_local_execute()`. This provides concurrency, but not parallelization, fitting nicely on top of the lightweight `mnvkd` virtual kernel, or other execution environments, like WebAssembly, where code and memory size are a serious concern.

The foundational coroutine interfaces are tested and demonstrated firstly in this local executor, since a full kernel is not needed.

### Logging API

[`vk_thread.h`](vk_thread.h):
Logs are sent to `stderr`, prefixing the message with `PRvk` (the format string) and `ARGvk(that)` (the arguments to the format string), providing thread context to logging messages.

#### Write to `stderr`
- `vk_logf(fmt, ...)`: log a formatted string
- `vk_log(str)`: log a string
- `vk_perror(str)`: log a string followed by the `strerror(errno)` message

#### Write to `stderr` if `DEBUG` is defined
- `vk_dbgf(fmt, ...)`: log a formatted string if `DEBUG` is defined
- `vk_dbg(str)`: log a string if `DEBUG` is defined
- `vk_dbg_perror(str)`: log a string followed by the `strerror(errno)` message if `DEBUG` is defined

#### Minimal Example

From [`vk_test_log.c`](vk_test_log.c):
```c
#define DEBUG 1

#include "vk_main_local.h"

void logging(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_logf("LOG test %d\n", 1);
	vk_log("LOG test");
	errno = EINVAL;
	vk_perror("LOG test");

	vk_dbgf("LOG debug %d\n", 1);
	vk_dbg("LOG debug");
	errno = EINVAL;
	vk_dbg_perror("LOG debug");

	vk_end();
}

int main() {
	return vk_local_main_init(logging, NULL, 0, 34);
}
```

### Memory API
[`vk_thread_mem.h`](vk_thread_mem.h):
- `vk_calloc(val_ptr, nmemb)`: stack-based allocation off the micro-heap
- `vk_calloc_size(val_ptr, nmemb, size)`: like `vk_calloc()`, but with an explicit size
- `vk_free()`: free the allocation at the top of the stack

When allocations would pass the edge of the micro-heap, an `ENOMEM` error is raised, and a log entry notes how many pages the micro-heap would have needed for the allocation to succeed. Starting with one page of memory, and increasing as needed, makes it easy to align heap sizes with code memory usage before compile time. All allocations are page-aligned, and fragments are only at the ends of pages. No garbage nor fragments can accumulate over time. Any memory leak would be obvious.

#### Minimal Example
From [`vk_thread_mem.h`](vk_thread_mem.h):
```c
#include <stdio.h>

#include "vk_main_local.h"

void example(struct vk_thread *that) {
	struct {
		struct blah {
			/* members dynamically allocated */
			int i;
			char *str; /* allocated inside the loop */
		} *blah_ptr;
		int j;
	} *self;
	vk_begin();

	vk_calloc(self->blah_ptr, 2);

	self->j = 0;
	for (self->j = 0; self->j < 2; self->j++) {

		/* use blah_ptr[self->j] */

		vk_calloc(self->blah_ptr[self->j].str, 50);
		snprintf(self->blah_ptr[self->j].str, 50 - 1, "Dynamically allocated for %i",
		         self->j);

		for (self->blah_ptr[self->j].i = 0;
		     self->blah_ptr[self->j].i < 10; self->blah_ptr[self->j].i++) {
			vk_logf("LOG counters: (%i, %i, %s)\n", self->j, self->blah_ptr[self->j].i,
			        self->blah_ptr[self->j].str);
		}

		vk_free(); /* blah_ptr[self->j].str was at the top of the stack */

	}

	vk_free(); /* self->blah_ptr was on the top of the stack */

	vk_end();
}

int main() {
	return vk_local_main_init(example, NULL, 0, 34);
}
```

### Execution API
[`vk_thread_exec.h`](vk_thread_exec.h)

#### Coroutine Execution Passing
- `vk_play()`: add the specified coroutine to the process run queue
- `vk_pause()`: stop the current coroutine
- `vk_call(there)`: transfer execution to the specified coroutine: `vk_pause()`, then `vk_play()`
- `vk_wait(socket_ptr)`: pause execution to poll for specified socket

#### Thread Creation
- `vk_child(child, vk_func)`: initialize a specified thread object of the specified thread function, inheriting the default socket
- `vk_responder(child, vk_func)`: initialize a specified thread object of the specified thread function, but connect the stdout of the parent to the stdin of the child
- `vk_accepted(there, vk_func, rx_fd_arg, tx_fd_arg)`: initialize a specified thread object of a specified thread function, but connect its default socket to the specified `struct vk_pipe` pipe pair.

#### Minimal Example

From [`vk_test_exec.c`](vk_test_exec.c):
```c
#include <stdio.h>

#include "vk_main_local.h"

void example2(struct vk_thread *that);

void example1(struct vk_thread *that)
{
	struct {
		struct vk_thread* example2_ptr;
		int i;
	}* self;
	vk_begin();

	vk_calloc_size(self->example2_ptr, 1, vk_alloc_size());
	vk_child(self->example2_ptr, example2);
	vk_copy_arg(self->example2_ptr, &that, sizeof (that));

	for (self->i = 0; self->i < 10; self->i++) {
		vk_logf("LOG example1: %i\n", self->i);
		vk_call(self->example2_ptr);
	}

	vk_free(); /* free self->example2_ptr */

	vk_end();
}

void example2(struct vk_thread *that)
{
	struct {
		struct vk_thread* example1_vk; /* other coroutine passed as argument */
		int i;
	}* self;
	vk_begin();

	for (self->i = 0; self->i < 10; self->i++) {
		vk_logf("LOG example2: %i\n", self->i);
		vk_call(self->example1_vk);
	}

	vk_end();
}

int main() {
	return vk_local_main_init(example1, NULL, 0, 34);
}
```

This pair of coroutines pass control back and forth to each other. Since memory allocation is stack-oriented, each coroutine allocates the next. Each coroutine needs to execute to allocate its own `*self` state. `vk_copy_arg()` copies the argument to the top of `*self` of the target coroutine.

### Future API
[`vk_thread_ft.h`](vk_thread_ft.h)

#### Synchronous Call

##### Caller
- `vk_request(there, send_ft_ptr, send_msg, recv_ft_ptr, recv_msg)`: `vk_call()` a coroutine, sending it a message, pausing the current coroutine until the callee sends a message back

##### Callee
- `vk_listen(recv_ft_ptr)`: wait for a `vk_request()` message
- `vk_respond(send_ft_ptr)`: reply a message back to the `vk_request()`, after processing the `vk_listen()`ed message

#### Asynchronous Call

- `vk_send(there, send_ft_ptr, send_msg)`: use specified future to send specified message to specified coroutine -- don't wait for a response
- Pair with `vk_listen()` to receive. See example below for a usage pattern.

#### Thread Creation
- `vk_child(there, vk_func)`: create a new child coroutine thread in the current process heap, the child inheriting the parent's default socket
  - FD in &rarr; parent &rarr; FD out
  - FD in &rarr; child &rarr; FD out

- `vk_responder(there, vk_func)`: create a new child coroutine thread in the current process heap, and connect the caller's `stdout` to the child's `stdin`, creating a pipeline similar to a Unix process pipeline
  - FD in &rarr; parent &rarr; child &rarr; FD out

After a child is created, it needs to be started with `vk_play()`, or something else that calls `vk_play()` on it, which enqueues the child in the run queue. For example, `vk_send()` will start the child asynchronously, with a future message waiting for it, and `vk_request()` will start a child synchronously, with a future message waiting for it, then wait for the child to reply back.

#### Combinatorial Snippets

Initialize and add to the run queue:
```c
vk_child(&self->child, ...);
vk_play(&self->child);
```

Initialize with a message:
```c
vk_child(&self->child, ...);
vk_send(&self->child, &self->future, msg_ptr);
```

Initialize with a message, waiting for a reply:
```c
vk_responder(&self->child, ...);
vk_request(&self->child, &self->future, &self->request, &self->response);
```

Each case of `vk_child()` can be replaced with `vk_responder()` instead.

Initialize and add to the run queue:
```c
vk_responder(&self->child, ...);
vk_play(&self->child);
```

Initialize with a message:
```c
vk_responder(&self->child, ...);
vk_send(&self->child, &self->future, msg_ptr);
```

Initialize with a message, waiting for a reply:
```c
vk_responder(&self->child, ...);
vk_request(&self->child, &self->future, &self->request, &self->response);
```

#### Minimal Examples

##### Synchronous Call

Parent `vk_request()` to child `vk_listen()` and `vk_respond()`.

From [`vk_thread_ft.c`](vk_thread_ft.c):
```c
#include <stdio.h>

#include "vk_main_local.h"
#include "vk_future_s.h"

void responder(struct vk_thread *that);

void requestor(struct vk_thread *that)
{
	struct {
		struct vk_future request_ft;
		struct vk_thread* response_vk_ptr;
		int request_i;
		int* response_i_ptr;
	}* self;
	vk_begin();

	self->request_i = 5;
	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());
	vk_child(self->response_vk_ptr, responder);

	/*
	 * parent::vk_request -> child::vk_listen -> child::vk_respond -> parent::vk_request
	 */

	vk_logf("LOG Request at requestor: %i\n", self->request_i);

	vk_request(self->response_vk_ptr, &self->request_ft, &self->request_i, self->response_i_ptr);

	vk_logf("LOG Response at requestor: %i\n", *self->response_i_ptr);

	vk_free(); /* free self->response_vk_ptr */

	vk_end();
}

void responder(struct vk_thread *that)
{
	struct {
		struct vk_future* parent_ft_ptr;
		int* request_i_ptr;
		int response_i;
	}* self;
	vk_begin();

	/*
	 * parent::vk_request -> child::vk_listen -> child::vk_respond -> parent::vk_request
	 */
	vk_listen(self->parent_ft_ptr, self->request_i_ptr);

	vk_logf("LOG Request at responder: %i\n", *self->request_i_ptr);

	self->response_i = (*self->request_i_ptr) + 2;

	vk_logf("LOG Response at responder: %i\n", self->response_i);

	vk_respond(self->parent_ft_ptr, &self->response_i);

	vk_end();
}

int main() {
	return vk_local_main_init(requestor, NULL, 0, 34);
}
```

##### Asynchronous Call

Parent `vk_send()` and `vk_listen()` to child `vk_listen()` and `vk_send()`.

From [`vk_thread_ft2.c`](vk_thread_ft2.c):
```c
#include <stdio.h>

#include "vk_main_local.h"
#include "vk_future_s.h"

void responder(struct vk_thread *that);

void requestor(struct vk_thread *that)
{
	struct {
		struct vk_future request_ft;
		struct vk_future* response_ft_ptr; /* only needed for vk_listen(), not vk_request() */
		struct vk_thread* response_vk_ptr;
		int request_i;
		int* response_i_ptr;
	}* self;
	vk_begin();

	self->request_i = 5;
	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());
	vk_child(self->response_vk_ptr, responder);

	/*
	 * parent::vk_send -> child::vk_listen -> child::vk_send -> parent::vk_listen
	 */
	vk_logf("LOG Request at requestor: %i\n", self->request_i);

	vk_send(self->response_vk_ptr, &self->request_ft, &self->request_i);
	vk_listen(self->response_ft_ptr, self->response_i_ptr);

	vk_logf("LOG Response at requestor: %i\n", *self->response_i_ptr);

	vk_free(); /* free self->response_vk_ptr */

	vk_end();
}

void responder(struct vk_thread *that)
{
	struct {
		struct vk_future* parent_ft_ptr;
		int* request_i_ptr;
		int response_i;
	}* self;
	vk_begin();

	/*
	 * parent::vk_send -> child::vk_listen -> child::vk_send -> parent::vk_listen
	 */
	vk_listen(self->parent_ft_ptr, self->request_i_ptr);

	vk_logf("LOG Request at responder: %i\n", *self->request_i_ptr);

	self->response_i = (*self->request_i_ptr) + 2;

	vk_logf("LOG Response at responder: %i\n", self->response_i);

	vk_send(self->parent_ft_ptr->vk, self->parent_ft_ptr, &self->response_i);

	vk_end();
}

int main() {
	return vk_local_main_init(requestor, NULL, 0, 34);
}
```

##### No Call (Background)

From [`vk_thread_ft3.c`](vk_thread_ft3.c):
```c
#include <stdio.h>

#include "vk_main_local.h"

void background(struct vk_thread *that);

void foreground(struct vk_thread *that)
{
	struct {
		struct vk_thread* background_vk_ptr;
	}* self;
	vk_begin();

	vk_calloc_size(self->background_vk_ptr, 1, vk_alloc_size());
	vk_child(self->background_vk_ptr, background);
	vk_play(self->background_vk_ptr);

	vk_log("LOG foreground");

	vk_pause();

	vk_log("LOG not reached");

	vk_free(); /* free self->background_vk_ptr */

	vk_end();
}

void background(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_log("LOG background");

	vk_end();
}

int main() {
	return vk_local_main_init(foreground, NULL, 0, 34);
}
```

### Exception API

The nested yields provide a very simple way to build a zero-overhead framework idiomatic of higher-level C-like languages (with exceptions), but with the simplicity and power of pure C. Nothing gets in the way of pure C.

[`vk_thread_err.h`](vk_thread_err.h):

#### Exception Handling
- `vk_raise(e)`, `vk_raise_at(there, e)`: raise the specified error, jumping to the `vk_finally()` label
- `vk_error()`, `vk_error_at(there)`: raise `errno` with `vk_raise()`
- `vk_finally()`: where raised errors jump to
- `vk_lower()`: within the `vk_finally()` section, jump back to the `vk_raise()` that jumped

Errors can yield via `vk_raise(error)` or `vk_error()`, but instead of yielding back to the same execution point, they yield to a `vk_finally()` label. A coroutine can only have a single finally label for all cleanup code, but the cleanup code can branch and yield `vk_lower()` to lower back to where the error was raised. High-level blocking operations raise errors automatically.

#### Signal Handling
- `vk_snfault(str, len)`, `vk_snfault_at(there, str, len)`: populate buffer with fault signal description for specified thread
- `vk_get_signal()`, `vk_get_signal_at(there)`: if `errno` is `EFAULT`, there may be a caught hardware signal, like `SIGSEGV`
- `vk_clear_signal()`: clear signal from the process
- `vk_sigerror()`: if there is a raised signal, log it

The virtual kernel signal handler handles a few signals, but ones that it does not, it checks to see if a coroutine was currently running, and if so, it raises EFAULT (either to the currently running coroutine, or the configured supervisor coroutine) and sets the signal info at `struct vk_proc_local::{siginfo,uc_ptr}`, which `vk_snfault()` and `vk_get_signal()` return. This allows segfaults and hardware traps to be caught and raised to a coroutine signal handler.

#### Supervision

- `vk_proc_local_set_supervisor(vk_get_proc_local(that)), there)`: assign `there` coroutine as the supervisor coroutine for this micro-process. Any signals will be raised to that coroutine instead of the currently running coroutine.

This enables Erlang's OTP-style supervision. Of course, since this errors are not raised by the coroutine itself, the state for `vk_lower()` is not set by a `vk_raise()`, so a `vk_lower()` would just go back to the last yield.

In theory, `void vk_set_error_ctx(struct vk_thread* that, int error)` can be used to explicitly set a lowering point for a `vk_lower()`, acting as a sort of "try". However, there is no "try", because `vk_lower()` counters the `vk_raise()`, so it would probably need a "retry", and another bit of context on the coroutine object. It is not difficult to try out different patterns here since this is just macro work, not compiler work.

#### Minimal Example

From [`vk_test_err.c`](vk_test_err.c):
```c
#include <stdio.h>

#include "vk_main_local.h"

void erring(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_log("ERR Before error is raised.");
	errno = ENOENT;
	vk_perror("ERR somefunction");
	vk_error();

	vk_log("ERR After error is raised.");

	vk_finally();
	if (errno != 0) {
		vk_perror("ERR Caught error");
		vk_lower();
	} else {
		vk_log("ERR Error is cleaned up.");
	}

	vk_end();
}

int main() {
	return vk_local_main_init(erring, NULL, 0, 34);
}
```

### Socket API

`struct vk_socket`: Userland Socket
- `vk_socket.h`
- `vk_socket_s.h`
- `vk_socket.c`

`struct vk_pipe`: Userland Pipe
- `vk_pipe.h`
- `vk_pipe_s.h`
- `vk_pipe.c`

Each coroutine may have a default socket object that represents its Standard I/O. This socket may be bound to physical file descriptors, or be logically entangled with other sockets, as inter-coroutine message passing channels. These bindings are represented as a Pipe that holds either a file descriptor or a pointer to a socket's receive or send queue. The same blocking ops that work against file descriptors also work across these inter-coroutine channels, in which case data is simply spliced to and from socket queues.

#### I/O API in `vk_thread_io.h`
##### Blocking Read Operations
- `vk_read(rc_arg, buf_arg, len_arg)`: read a fixed number of bytes into a buffer, or until EOF (set by `vk_hup()`)
- `vk_readline(rc_arg, buf_arg, len_arg)`: read a fixed number of bytes into a buffer, or until EOF (set by `vk_hup()`) or a newline character

###### Minimal Example
From [`vk_test_read.c`](vk_test_read.c):
```c
#include <stdio.h>

#include "vk_main_local.h"

void reading(struct vk_thread *that)
{
	int rc = 0;

	struct {
		char line[1024];
		char buf[1024];
		size_t size;
	}* self;
	vk_begin();

	vk_readline(rc, self->line, sizeof (self->line) - 1);
	if (strcmp(self->line, "MyProto 1.0\n") != 0) {
		vk_raise(EINVAL);
	}

	vk_readline(rc, self->line, sizeof (self->line) - 1);
	rc = sscanf(self->line, "%zu", &self->size);
	if (rc != 1) {
		vk_perror("sscanf");
		vk_error();
	}

	if (self->size > sizeof (self->buf) - 1) {
		vk_raise(ERANGE);
	}

	vk_logf("LOG header size: %zu\n", self->size);

	vk_read(rc, self->buf, self->size);
	if (rc != self->size) {
		vk_raise(EPIPE);
	}

	vk_logf("LOG body: %s\n", self->buf);

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG reading");
	}

	vk_end();
}

int main() {
	return vk_local_main_init(reading, NULL, 0, 34);
}
```

##### Blocking Write Operations
- `vk_write(buf_arg, len_arg)`: write a fixed number of bytes from a buffer
- `vk_writef(rc_arg, line_arg, line_len, fmt_arg, ...)`: write a fixed number of bytes from a format string buffer
- `vk_write_literal(literal_arg)`: write a literal string (size determined at build time)
- `vk_flush()`: block until everything written has been physically sent

###### Minimal Example
From [`vk_test_write.c`](vk_test_write.c):
```c
#include <stdio.h>

#include "vk_main_local.h"

void writing(struct vk_thread *that)
{
	int rc = 0;
	struct {
		char line[1024];
		char buf[1024];
		size_t size;
	}* self;
	vk_begin();

	strncpy(self->buf, "1234567890", sizeof (self->buf) - 1);
	self->size = strlen(self->buf);

	vk_write_literal("MyProto 1.0\n");
	vk_writef(rc, self->line, sizeof (self->line) - 1, "%zu\n", self->size);
	vk_write(self->buf, self->size);
	vk_flush();

	vk_finally();
	if (errno != 0) {
		vk_perror("writing");
	}

	vk_end();
}

int main() {
	return vk_local_main_init(writing, NULL, 0, 34);
}
```

##### Blocking Splice Operation
- `vk_forward(rc_arg, len_arg)`: read a specified number of bytes, up to EOF, immediately writing anything that is read (does not flush itself)

For example, when implementing a forwarding proxy, the headers will likely want to be manipulated directly, but chunks of the body may be sent untouched. This enables the chunks of the body to be sent without need for an intermediate buffer.

##### Blocking Accept Operation
- `vk_accept(accepted_fd_arg, accepted_ptr)`: accept a file descriptor from a listening socket, for initializing a new coroutine (used by `vk_service`)

This simply reads a `struct vk_accepted` object from the socket, which is a message that holds the accepted FD and its peer address information returned in the underlying `accept()` call. That is, at the physical layer, accepts are performed "greedy", but the higher levels may elect to implement a more "lazy" processing to preserve the latency of other operations.

The FD reported in `struct vk_accepted` is placed in `accepted_fd_arg` for convenience.

##### Blocking EOF (End-of-File) / HUP (Hang-UP) Operations
- `vk_pollhup()`: waits for EOF available to be read -- blocks until `vk_readhup()` would succeed
- `vk_readhup()`: reads an EOF -- blocks until the writer of this reader does a `vk_hup()`
- `vk_hup()`: writes an EOF -- blocks until the reader of this writer does a `vk_readhup()`, and EOF is cleared by this op automatically

That is:
1. writer/producer writes a message, then uses `vk_hup()` to terminate the message (marking EOF), which will return when the other size acknowledges, reading the EOF.
2. reader/consumer reads a message until `vk_pollhup()` (when the writer marks EOF), then processes the message however it likes, then uses `vk_readhup()` (to move the EOF from the writer to the reader) to signal to the reader/producer that the message has been processed.

This allows a synchronous/transactional interface between a producer and a consumer, without the producer needing to block on a return. Much of the time, only a boolean acknowledgement is needed, because the consumer can always raise an error directly to the producer with `vk_raise_at()`.

This also provides the semantics needed for sending datagrams or other message-oriented protocols. That is, for protocols where a `sendmsg()` or `recvmsg()` is needed by the OS sockets, this is the interface to those operations.

###### Producer
```c
/*
 * Producer
 */
for (;;) {
	/* write message */

	vk_hup();
	/*
	 * At this point, the message writes are flushed, and
	 * the consumer has acknowledged with a `vk_readhup()`.
	 */
}
```

###### Consumer
```c
/*
 * Consumer
 */
for (;;) {
	while (!vk_pollhup()) {
		/* read parts of message */
	}
	/*
	 * At this point, the complete message has been read, but the consumer is still blocked on `vk_hup()`.
	 *
	 * Perform any transactional processing that needs to be done before the producer continues past `vk_hup()`.
	 */
	vk_readhup();
	/*
	 * At this point, the producer can continue past `vk_hup()` to produce the next message.
	 */
}
```

##### Blocking Close Operations
- `vk_rx_close()`: close the read side of the socket
- `vk_tx_close()`: close the write side of the socket
- `vk_rx_shutdown()`: shutdown the read side of the socket (FD remains open)
- `vk_tx_shutdown()`: shutdown the write side of the socket (FD remains open)

##### Blocking Direct Polling Operations
- `vk_again()`: handle an EAGAIN (try again later) message -- enqueue for block
- `vk_readable()`: wait for socket to be readable (poll event)
- `vk_writable()`: wait for socket to be writable (poll event)

##### Operation Internals
For each `vk_*()` op, there is an equal `vk_socket_*()` op that operates on a specified socket, rather than the coroutine default socket. In fact, the `vk_*()` I/O API is simply a macro wrapper around the `vk_socket_*()` API that passes the default socket `socket_ptr` as the first argument.

Each blocking operation is built on:
1. `vk_wait()`, which pauses the coroutine to be awakened by the network poller,
2. the `vk_socket_*()` interface in `vk_socket.h`, which holds vectoring and block objects, consumed by:
3. the `vk_vectoring_*()` interface in `vk_vectoring.h`, which gives and takes data from higher-level I/O ring buffer queues, and
4. the `vk_block_*()` interface in `vk_socket.h`, which controls signals the lower-level, physical socket operations.

## Virtual Kernel Interfaces

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

A coroutine may yield a future directly to another coroutine, passing a reference directly to memory, rather than buffering data in a socket queue.

A parent coroutine can spawn a child coroutine, where the child inherits the parent's socket. Otherwise, a special "pipeline" child can be used, which, on start, creates pipes to the parent like a unix pipeline. That is, the parent's standard output is given to the child, and the parent's standard output is instead piped to the child's standard input.  A normal child uses `vk_begin()` just like a normal coroutine, and a pipeline child uses `vk_begin_pipeline(future)`, which sets up the pipeline to the parent, and receives an initialization future message. All coroutines end with `vk_end()`, which is required to end the state machine's switch statement, and to free `self` and the default socket allocated by `vk_begin()`.

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
- fresh: Events were returned by the poller, so the blocked process needs to continue.

The polling time lifecycle state is kept:
1. pre: needs to be registered with the poller, "dirty" flushing to:
2. post: currently registered with the poller, "fresh" flushing to:
3. ret: dispatch to the process, to continue the blocked virtual thread.

A network poller device simply registers the dirty FDs, and returns fresh FDs.
