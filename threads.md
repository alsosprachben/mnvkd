# `mnvkd` Threading Interface

## Coroutines

### Stackless Coroutine Micro-Threads
`struct vk_thread`:
- [`Coroutine Thread Object Doc`](objects.md#coroutine-thread-object)
- [`vk_thread.h`](vk_thread.h): public interface
- [`vk_thread_s.h`](vk_thread_s.h): private struct
- [`vk_thread.c`](vk_thread.c): implementation

#### Blocking Macros
- [`vk_thread_cr.h`](vk_thread_cr.h): [Coroutine API](#coroutine-api)
- [`vk_thread.h`](vk_thread.h): [Logging API](#logging-api)
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
	return vk_main_local_init(example, NULL, 0, 26);
}
```

The `VK_PROC_YIELD` state tells the execution loop to place the thread back in `VK_PROC_RUN` state, ready to be enqueued in the run queue. This is actually how `vk_pause()` is implemented, which is part of how `vk_call()` is implemented. This demonstrates how being able to nest with `__COUNTER__` enables a clean, higher-level interface that is more thread-like, and easy to extend.

##### Local Coroutine Executor
`vk_main_local.h`:
- `vk_main_local_init(main_vk, buf, buflen, page_count)`
  - `main_vk`: The coroutine function.
  - `buf`: pointer to buffer to copy into the beginning of `self`, as an argument to the coroutine.
  - `buflen`: the number of bytes to copy.
  - `page_count`: the number of 4096 byte pages to allocate for the coroutine's micro-heap.

This interface uses only the micro-process local interfaces, which run in the micro-heap. No virtual kernel is running, so no polling nor inter-micro-process scheduling is done. This is mainly for testing. It binds `stdin` and `stdout` to the coroutine's default socket, in blocking mode, since there is no polling. It runs the coroutine once, until it yields or blocks.

This provides a stateful thread library runtime as a single function that iterates over the run queue: `vk_proc_local.c:vk_proc_local_execute()`. This provides concurrency, but not parallelization, fitting nicely on top of the lightweight `mnvkd` virtual kernel, or other execution environments, like WebAssembly, where code and memory size are a serious concern.

The foundational coroutine interfaces are tested and demonstrated firstly in this local executor, since a full kernel is not needed.

Most test examples shown here simply use this local coroutine executor.

##### Full Coroutine Executor
`vk_main.h`:
- `vk_main_init(main_vk, buf, buflen, page_count, privileged, isolated)`
  - `main_vk`: The coroutine function.
  - `buf`: pointer to buffer to copy into the beginning of `self`, as an argument to the coroutine.
  - `buflen`: the number of bytes to copy.
  - `page_count`: the number of 4096 byte pages to allocate for the coroutine's micro-heap.
  - `privileged`: whether the coroutine should run in privileged mode: virtual kernel is protected while running.
  - `isolated`: whether the coroutine should run in isolated mode: itself is protected while not running.

This interface uses the full virtual kernel runtime. It binds `stdin` and `stdout` to the coroutine's default socket, in non-blocking mode, since there is polling.

It also runs `vk_proc_local.c:vk_proc_local_execute()`, but within a `struct vk_proc` micro-process, managed by the `struct vk_kern` virtual kernel. When local execution has gone as far as it can, the virtual kernel either exits the process, or registers any blocks with the poller, waking the micro-process when any of the blocked resources are ready.

This provides concurrency via micro-process scheduling, and parallelization via network polling. The unified messaging interface provided by `struct vk_socket` enables an actor-like usage.

The `privileged=0` and `isolated=1` flags enable memory protection, and signal handling is enabled.

Some more-complex test examples shown here use this full coroutine executor.

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
	return vk_main_local_init(logging, NULL, 0, 34);
}
```

##### Test Harness Pattern

In the examples in this document, all log entries will begin with `LOG` to give the test harness an easy way to filter out the contextual information prior to the actual log message.

For example, the test harness for proir example is the following set of Makefile targets from [`Makefile`](Makefile):
```makefile
# vk_test_log
vk_test_log.out.txt: vk_test_log
	./vk_test_log 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_log.out.txt

vk_test_log.valid.txt:
	cp vk_test_log.out.txt vk_test_log.valid.txt

vk_test_log.passed: vk_test_log.out.txt vk_test_log.valid.txt
	diff -q vk_test_log.out.txt vk_test_log.valid.txt && touch "${@}"
```

Notice how the `grep` and `sed` commands filter out the `LOG` prefix, so that the actual log message is compared, not the context. This allows tests to operate properly:
1. despite running in verbose debug mode, which includes many system-level log entries, and
2. ignores changes in informational context prepended to the log entries.

The `diff -q` command will return a non-zero exit code if the test output and the valid output reference differ, and a zero exit code if they are the same. The `touch` command will create the `vk_test_log.passed` file if the `diff` command returns a zero exit code, indicating that the test passed. The `*.passed` files are the dependent targets of the meta `test` target. Therefore, even the tests use the principle of "inversion of control".

In this test, the valid output of the test is from [`vk_test_log.valid.txt`](vk_test_log.valid.txt):
```text
test 1
test
test: Invalid argument
debug 1
debug
debug: Invalid argument
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
	return vk_main_local_init(example, NULL, 0, 34);
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
	return vk_main_local_init(example1, NULL, 0, 34);
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
- `vk_recv(recv_msg)`: retrieve the response sent from `vk_send()`, assuming it is waiting, no implicit pause.

If using it synchronously, there must be a `vk_pause()` between the send and receive to emulate the full `vk_request()` behavior. See example below.

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

From [`vk_test_ft.c`](vk_test_ft.c):
```c
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
	return vk_main_local_init(requestor, NULL, 0, 34);
}
```

##### Asynchronous Call

Parent `vk_send()`, `vk_pause()` and `vk_recv()` to child `vk_listen()` and `vk_send()`. Could have used `vk_listen()` instead of `vk_recv()` in the parent. Could have used `vk_respond()` instead of `vk_send()` in the child. This shows how the semantics relate to the synchronous call.

From [`vk_test_ft2.c`](vk_test_ft2.c):
```c
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
	 * parent::vk_send -> child::vk_listen -> child::vk_send -> parent::vk_listen
	 */
	vk_logf("LOG Request at requestor: %i\n", self->request_i);

	vk_send(self->response_vk_ptr, &self->request_ft, &self->request_i);
	vk_pause();
	vk_recv(self->response_i_ptr);

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
	return vk_main_local_init(requestor, NULL, 0, 34);
}
```

##### No Call (Background)

From [`vk_test_ft3.c`](vk_test_ft3.c):
```c
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
	return vk_main_local_init(foreground, NULL, 0, 34);
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

The `struct vk_signal` from [`vk_signal.c`](vk_signal.c) is used by the `struct vk_kern` object to handle signals. See the bottom of that file for the unit test for the `struct vk_signal` object within the `#ifdef VK_SIGNAL_TEST`.

#### Supervision

- `vk_proc_local_set_supervisor(vk_get_proc_local(that)), there)`: assign `there` coroutine as the supervisor coroutine for this micro-process. Any signals will be raised to that coroutine instead of the currently running coroutine.

This enables Erlang's OTP-style supervision. Of course, since signal errors are not raised by the coroutine itself, the state for `vk_lower()` is not set by a `vk_raise()`, so a `vk_lower()` would just go back to the last yield.

In theory, `void vk_set_error_ctx(struct vk_thread* that, int error)` can be used to explicitly set a lowering point for a `vk_lower()`, acting as a sort of "try". However, there is no "try", because `vk_lower()` counters the `vk_raise()`, so it would probably need a "retry", and another bit of context on the coroutine object. It is not difficult to try out different patterns here since this is just macro work, not compiler work.

#### Minimal Examples

From [`vk_test_err.c`](vk_test_err.c):
```c
#include "vk_main_local.h"

void erring(struct vk_thread *that)
{
	struct {
	}* self;
	vk_begin();

	vk_log("LOG Before error is raised.");
	errno = ENOENT;
	vk_perror("LOG somefunction");
	vk_error();

	vk_log("LOG After error is raised.");

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG Caught error");
		vk_lower();
	} else {
		vk_log("LOG Error is cleaned up.");
	}

	vk_end();
}

int main() {
	return vk_main_local_init(erring, NULL, 0, 34);
}
```

From [`vk_test_err2.c`](vk_test_err2.c):
```c
#include "vk_main.h"
#include "vk_thread.h"

void erring2(struct vk_thread *that)
{
	volatile ssize_t rc = 0; /* volatile to avoid optimization */
	struct {
		int erred;
	}* self;
	vk_begin();

	vk_log("LOG Before signal is raised.");

	if ( ! self->erred) {
		vk_log("LOG Generating signal.");
		rc = 0;
		rc = 5 / 0; /* raise SIGFPE */
	} else {
		vk_log("LOG Signal is not generated.");
	}

	vk_log("LOG After signal is raised.");

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG Caught signal");
		vk_sigerror();
		self->erred = 1;
		vk_lower();
	} else {
		vk_log("LOG Signal is cleaned up.");
	}

	vk_end();
}

int main() {
	return vk_main_init(erring2, NULL, 0, 26, 0, 1);
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
	return vk_main_local_init(reading, NULL, 0, 34);
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
	return vk_main_local_init(writing, NULL, 0, 34);
}
```

##### Blocking Splice Operation
- `vk_forward(rc_arg, len_arg)`: read a specified number of bytes, up to EOF, immediately writing anything that is read (does not flush itself)

For example, when implementing a forwarding proxy, the headers will likely want to be manipulated directly, but chunks of the body may be sent untouched. This enables the chunks of the body to be sent without need for an intermediate buffer.

##### Minimal Example
From [`vk_test_forward.c`](vk_test_forward.c):
```c
#include "vk_main_local.h"

void forwarding(struct vk_thread *that)
{
	int rc = 0;
	struct {
		char line[1024];
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

	vk_logf("LOG header size: %zu\n", self->size);

	vk_write_literal("MyProto 1.0\n");
	vk_writef(rc, self->line, sizeof (self->line) - 1, "%zu\n", self->size);

	vk_forward(rc, self->size);
	vk_flush();

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG forwarding");
	}

	vk_end();
}

int main() {
	return vk_main_local_init(forwarding, NULL, 0, 34);
}
```


##### Blocking EOF (End-of-File) / HUP (Hang-UP) Operations
- `vk_pollhup()`: non-blocking check for EOF available to be read -- whether a `vk_readhup()` would succeed
- `vk_readhup()`: non-blocking reads an EOF -- blocks until the writer of this reader does a `vk_hup()`
- `vk_hup()`: writes an EOF -- blocks until the reader of this writer does a `vk_readhup()`, and EOF is cleared by this op automatically

That is:
1. writer/producer writes a message, then uses `vk_hup()` to terminate the message (marking EOF), which will return when the consumer acknowledges, reading the EOF via `vk_readhup()`.
2. reader/consumer reads a message until `vk_pollhup()` (when the writer marks EOF), then processes the message however it likes, then uses `vk_readhup()` (to move the EOF from the writer to the reader) to signal to the reader/producer that the message has been processed.

This allows a synchronous/transactional interface between a producer and a consumer, without the producer needing to block on a return. Much of the time, only a boolean acknowledgement is needed, because the consumer can always raise an error directly to the producer with `vk_raise_at()`.

By enabling an acknowledgement of a message boundary on a single direction of the socket, the other side of the socket is free for other purposes. Specifically, the typical pattern is that the input data is used to produce the output data, a "Standard I/O" interface. So a separate return channel would require a separate socket object, rather than a simple boolean flag on the underlying `struct vk_vectoring` objects. This allows for complete actor and CSP benefits, while controlling forward-pressure via the transactional block-unblock semantics of `vk_hup()` and `vk_readhup()`. The next section on `vk_pollread()` further enhances this control.

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

##### Polling Operation
- `vk_pollread(rc_arg)`: wait until any amount of bytes are available to be read, returning the number of bytes available -- does not yet perform a read

For example, this may be used to forward or process data as it comes in, where there is no explicit length in the protocol. The `vk_readline()` op can read lines as they come in, but it peeks at the buffer to find the newline delimiter. However, `vk_pollread()` allows the buffer to be easily peaked or partially processed without needing to build a special blocking operation. This is also used, for example, to package up chunks of bytes into HTTP chunks, without needing an intermediary buffer, by polling for a chunk of bytes, writing a header, forwarding them with `vk_forward()`, and writing a trailer.

##### Minimal Example
From [`vk_test_pollread.c`](vk_test_pollread.c):
```c
#include "vk_main.h"
#include "vk_thread.h"

void pollreading(struct vk_thread* that) {
	ssize_t rc = 0;
	struct {
		char line[1024];
		ssize_t size;
	} *self;

	vk_begin();

	vk_pollread(self->size);
	if (self->size > sizeof (self->line) - 1) {
		vk_raise(ERANGE);
	}
	vk_logf("LOG size of 1st chunk: %zu\n", self->size);

	vk_read(rc, self->line, self->size);
	if (rc != self->size) {
		vk_raise(EPIPE);
	}

	vk_pollread(self->size);
	if (self->size > sizeof (self->line) - 1) {
		vk_raise(ERANGE);
	}
	vk_logf("LOG size of 2nd chunk: %zu\n", self->size);

	vk_read(rc, self->line, self->size);
	if (rc != self->size) {
		vk_raise(EPIPE);
	}

	vk_finally();
	if (errno != 0) {
		vk_perror("LOG pollreading");
	}

	vk_end();
}

int main(int argc, char* argv[])
{
	return vk_main_init(pollreading, NULL, 0, 26, 0, 1);
}
```

##### Blocking Accept Operation
- `vk_accept(accepted_fd_arg, accepted_ptr)`: accept a file descriptor from a listening socket, for initializing a new coroutine (used by `vk_service`)

This simply reads a `struct vk_accepted` object from the socket, which is a message that holds the accepted FD and its peer address information returned in the underlying `accept()` call. That is, at the physical layer, accepts are performed "greedy", but the higher levels may elect to implement a more "lazy" processing to preserve the latency of other operations.

The FD reported in `struct vk_accepted` is placed in `accepted_fd_arg` for convenience.


##### Blocking Close Operations
- `vk_rx_close()`: close the read side of the socket
- `vk_tx_close()`: close the write side of the socket
- `vk_rx_shutdown()`: shutdown the read side of the socket (FD remains open)
- `vk_tx_shutdown()`: shutdown the write side of the socket (FD remains open)

##### Operation Internals
For each `vk_*()` op, there is an equal `vk_socket_*()` op that operates on a specified socket, rather than the coroutine default socket. In fact, the `vk_*()` I/O API is simply a macro wrapper around the `vk_socket_*()` API that passes the default socket `socket_ptr` as the first argument.

Each blocking operation is built on:
1. `vk_wait()`, which pauses the coroutine to be awakened by the network poller,
2. the `vk_socket_*()` interface in `vk_socket.h`, which holds vectoring and block objects, consumed by:
3. the `vk_vectoring_*()` interface in `vk_vectoring.h`, which gives and takes data from higher-level I/O ring buffer queues, and
4. the `vk_block_*()` interface in `vk_socket.h`, which controls signals the lower-level, physical socket operations.

