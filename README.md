# M:N Virtual Kernel Daemon

## Synopsis

`mnvkd` is a server authoring toolkit.
- `mnvkd` is for _servers_ what SQLite is for _databases_.
- What if writing a server had the interface of `inetd`, `cgi`, and forking servers of decades ago, but with optimal performance that beats modern frameworks?
- What if you could write *edge* functions with *cloud* function semantics?
- What if coroutines had a `stdio`, thread-like interface, with actor patterns?
- What if this were all real-time, lockless, and with almost no latency tail?

### `mnvkd` is a C service stack, comprised of *threading*, *actor*, *service*, and *application* frameworks:
1. a *threading* framework: An event-based, stackless, stateful-threaded coroutine framework with message passing and I/O aggregation facilities,
2. an *actor* framework: A soft-real-time, actor-based virtual kernel of micro-processes that drive the micro-thread coroutines,
3. a *service* framework: A "Super-Server" framework and protocol suite of micro-processes, and
4. an *application* framework: A cloud and edge application server and runtime library.

#### (#1) the `mnvkd` threading framework is an M:1, deterministic, stackless, stateful-threading library based on a novel coroutine framework:
1. providing deterministic concurrency,
2. within a single, small contiguous virtual memory space (micro-heap),
3. for extremely high locality of reference,
4. with message passing, and high-level, blocking I/O stream operations for automatically aggregating I/O operations.

#### (#2) the `mnvkd` actor framework is an M:N C platform alternative to Erlang and Golang to drive many M:1 processes:
1. providing run-time memory safety, but
2. without garbage collection,
3. without locking,
4. without a custom language, and
5. within an unfettered C environment.
6. Designed for:
    1. fault-tolerant,
    2. high-throughput,
    3. low-latency,
    4. high-efficiency,
    5. low-cost,
    6. distributed systems.

#### (#3) the `mnvkd` service framework is a "Super-Server" like `systemd`, `launchd`, and `inetd`:
1. but instead of being process-based, is event-based, while
2. preserving the simple interface of `inetd`,
3. preserving the privilege separation of `inetd` and `systemd`, and
4. preserving an unfettered C environment.
5. Designed for implementing distributed service protocols:
   1. HTTP servers and clients
   2. DNS
   3. Storage Services, like S3, Redis RESP, ElasticSearch, etc.

#### (#4) the `mnvkd` application framework seeks to provide a `Web-interoperable Runtime` library for cloud and edge services:
1. but instead of being JavaScript, it is C, where
2. interfaces are C-idiomatic variants of the [WinterTC](https://wintertc.org/) standard interfaces, with
3. `struct iovec` ring buffers, instead of "streams", with
4. stackless coroutines with _readiness-oriented_ Standard I/O blocking operations over the streams, instead of _completion-oriented_ async functions, and
5. _structured_ virtual processes in isolated micro-heaps with coroutine threads, instead of _unstructured_ futures, where
6. *binary code* in isolation can be used, instead of *WebAssembly* in isolation.
7. That is, WebAssembly can still be used, but *per-request containerization* already exists.

### `mnvkd` is built on novel technology:
1. A novel coroutine system.
2. A novel memory layout.
3. A novel scheduling system.
4. A novel memory protection system.
5. A novel service system.
6. A novel network polling system.

#### (#1) `mnvkd` is a proof of concept for a coroutine-based threading based on macros
1. The [stackless coroutine system](threads#coroutines) is derived from [Simon Tatham's coroutines](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html) but with a novel enhancement.
2. Preserving cache locality, and enabling dispatch aggregation, for extreme vertical efficiency.

#### (#2) `mnvkd` is a proof of concept for a novel memory layout:
1. where structured programming can also involve a structured memory layout that aligns data-structure with code-structure, dramatically reducing cache misses.
2. that reduces the need for large amounts of on-die cache, meaning that expensive server processors are no longer needed.
3. that greatly simplifies userland memory protection and scheduling.

#### (#3) `mnvkd` is a proof of concept for partitioned scheduling:
1. where data partitioning is extended to scheduling.
2. The micro-threads reside in micro-processes with micro-heaps, where the threads and processes are scheduled in 2 layers, like stateful threading libraries.
3. That is, cache-missing linear access only needs to apply to the partitioned structure, the virtual process,
4. meaning that cache-hitting, scanning access can be used locally, within the virtual process, to improve throughput.
5. That is, both thread scheduling and memory protection can benefit from an encapsulating locality of reference,
6. which is what makes in-process memory protection actually feasible.

#### (#4) `mnvkd` is a proof of concept for a novel memory protection:
1. where memory protection and privilege separation can be done in-process in C.
2. where a virtual kernel can be implemented in userland without a full kernel implementation, but rather a threading implementation using existing POSIX interfaces.
3. scoped in virtual processes, a third scheduling layer between a virtual kernel and virtual threads: M:N:1 scheduling.
4. The theory is simple:
    1. a regular threading library is simply an unprotected virtual kernel, therefore
    2. protecting a threading library makes a proper virtual kernel, therefore
    3. protecting a userland threading library (without kernel threads, with event-based userland scheduling) makes an extremely fast virtual kernel, therefore
    4. providing a memory-safe M:N processing solution.

#### (#5) `mnvkd` is a proof of concept for socket-based servers which use modern and historical system calls:
1. to approach the performance of bypassing the kernel network stack.
2. That is, modern stream-oriented polling, async I/O, ring buffer, and memory mapping interfaces
3. can reduce the system call frequency overhead to the noise floor,
4. providing the benefit of kernel support, and its security and isolation mechanisms.

See also: `isolation.md` for isolation, syscall gating via Linux SUD, privileged
page masking, and integration guidance.


#### (#6) `mnvkd` is a proof of concept for a novel deductive network poller:
1. to optimally reduce the number of I/O system calls.
2. where the poller knows when I/O will block, so it knows when to flush and poll.
3. where vector-based ring buffers are used to optimize I/O copying.



## Quick Start

### Docker Compose Benchmarks

1. `docker compose build`
2. `docker compose up`
3. Load [`fortio`'s report UI @ http://localhost:8085/](http://localhost:8085/)
4. In the `docker compose up` terminal, after fortio saves a report JSON file, reload the fortio UI page in step 3, and click on the file name to see the results of the run.

This runs `fortio` for 30, 1000, and 10000 connections, for 30 seconds on each of:
- `http11`: this container runs `vk_test_http11_service`, the HTTP/1.1 example Hello World service implemented at `vk_http11.c`, `vk_rfc.c`, `vk_test_http11_service.c` and `Dockerfile`.
- `httpexpress:`: this container runs a `node-express` example Hello World service implemented at `expresshw.js` and `Dockerfile.express`.
- `httpunit`: this container runs NGINX Unit, with an example JavaScript Hello World service implemented at `Dockerfile.unit`

The idea is to compare with other web application servers for API development.

### Executables

#### `vk_test_echo_cli`, `vk_test_echo_service`

From `vk_echo.c` via `vk_test_echo_cli.c` and `vk_test_echo_service.c`.

An echo server that simply outputs input lines prefixed with line number.

#### `vk_test_http11_cli`, `vk_test_http11_service`

From `vk_http11.c` and `vk_rfc.h`, via `vk_test_http11_cli.c` and `vk_test_http11_service.c`.

An HTTP/0.9,1.0,1.1 server, with a response handler `http11_response` using a simple SAPI of:
1. a request struct,
2. the streaming entity readable until `vk_readhup()`, and
3. the response directly written by the response handler -- that is, a raw response line and headers, no `Status` header like CGI.

The HTTP protocol handling uses:
1. the [threading interface](threads.md),
2. the [RFC utilities](vk_rfc.h) for RFC line (`\r\n`) parsing, and chunked encoding, and
3. the [Enum utilities](vk_enum.h) for HTTP constants.

#### `vk_test_redis_cli`, `vk_test_redis_service`

From `vk_redis.c` via `vk_test_redis_cli.c` and `vk_test_redis_service.c`.

A Redis RESP server that persists `SET` and `GET` commands in an SQLite database.

##### `vk_fetch`

From `vk_fetch.c`, a Fetch API implementation. Not completed yet. The HTTP request object will be migrated to the request object in the [Fetch API](https://fetch.spec.wintertc.org/) idiom, providing a cloud-function [WinterTC](https://wintertc.org/) standard interface.

### Building

#### with CMake

A `CMakeLists.txt` file is provided. This is mainly for IDE integration.

#### with BSD Make

A `Makefile` is provided, but it is a BSD Make flavor. On Ubuntu, use `apt install bmake`. See `Dockerfile` for a simple Linux build example.
Use `release.sh` or `debug.sh` wrappers to set release or debug environment. For example: `./debug.sh bmake` and `./debug.sh bmake clean`.

### Runtime Environment

#### Polling Methods

- `VK_POLL_DRIVER=OS`: Use the OS-specific poller rather than `poll()`. Note that `poll()` is used optimally, so it is not slow. Try with both.
- `VK_POLL_METHOD=EDGE_TRIGGERED`: Some pollers can be used in a more sophisticated way. This enables that.

These control polling methods. See the `vk_fd_table*` files:
- The top of `vk_fd_table_s.h` defines which poller to use:
  - `VK_USE_KQUEUE`: BSD `kqueue()`. `EDGE_TRIGGERED` will keep registrations set, but use one-shot enabling. Otherwise, registrations are added in a one-shot manner.
  - `VK_USE_EPOLL`: Linux `epoll()` and `epoll_ctl()`. `EDGE_TRIGGERED` will register once with edge-triggering. Otherwise, one-shot registrations are used each time, which uses more system calls.
  - `VK_USE_GETEVENTS`: Linux AIO `io_setup()`, `io_submit()` and `io_getevents()`.
  - else: POSIX `poll()`.
- The bottom of `vk_fd_table.c` contains the poll drivers, with extra needed state added to `vk_fd_table_s.h`. It is ridiculously easy to add a new poll driver. "Dirty" sockets need polling. Mark sockets "fresh" to be awakened. The `struct vk_io_future` objects contain poll state. The `pre` are prior to polling, `post` is currently polling, and `ret` is what is returned.

### Complete Example Echo Service
### Test Runner

For guarded test execution (timeouts + output caps) use:

- `tools/run-tests.sh` wraps Makefile targets safely to prevent runaway logs or hangs.
  - Quick examples:
    - `tools/run-tests.sh` (debug mode, runs `test`, 240s timeout, 400-line cap)
    - `tools/run-tests.sh safe` (curated subset of fast/stable tests)
    - `tools/run-tests.sh --mode release --timeout 120 --lines 300 --target vk_test_exec.passed`
  - Use `tools/run-tests.sh --list` to see the curated safe targets.

- [`vk_echo.c`](vk_echo.c): echo coroutine thread
- [`vk_test_echo_cli.c`](vk_test_echo_cli.c): CLI main for `vk_test_echo_cli` executable
- [`vk_test_echo_service.c`](vk_test_echo_service.c): Server main for `vk_test_echo_service` executable
- [`vk_test_echo.in.txt`](vk_test_echo.in.txt): test input
- [`vk_test_echo.valid.txt`](vk_test_echo.valid.txt): valid test output

```c
#include "vk_thread.h"
#include "vk_service_s.h"

void vk_echo(struct vk_thread* that)
{
	int rc = 0;

	struct {
		struct vk_service service; /* via vk_copy_arg() */
		size_t i;
		struct {
			char in[8192];
			char out[8192];
		}* buf; /* pointer to demo dynamic allocation */
	}* self;

	vk_begin();

	for (self->i = 0;; self->i++) {
		vk_calloc(self->buf, 1); /* demo dynamic allocation in a loop */

		vk_readline(rc, self->buf->in, sizeof(self->buf->in) - 1);
		if (rc == 0 || vk_eof()) {
			vk_free();
			break;
		}

		self->buf->in[rc] = '\0';

		rc = snprintf(self->buf->out, sizeof(self->buf->out) - 1, "Line %zu: %s", self->i, self->buf->in);
		if (rc == -1) {
			vk_error();
		}

		vk_write(self->buf->out, strlen(self->buf->out));
		vk_flush();

		vk_free(); /* demo deallocating each loop */
	}

	vk_end();
}
```

#### CLI Main Entrypoint

[`vk_main.h`](vk_main.h) starts a coroutine thread connected to `stdin` and `stdout`, as is done in [`vk_test_echo_cli.c`](vk_test_echo_cli.c).

```c
#include "vk_main.h"
#include "vk_echo.h"

int main(int argc, char* argv[])
{
	return vk_main_init(vk_echo, NULL, 0, 25, 0, 1);
}
```

- `vk_main_init(main_vk, arg_buf, arg_len, page_count, privileged, isolated)`
    - `main_vk`: coroutine function to bind to standard I/O.
    - `arg_buf`: an optional argument to copy to the beginning of the `self` struct, as an argument.
    - `arg_len`: the number of bytes to copy.
    - `page_count`: the number of 4096 byte pages to allocate for the process micro-heap. If a runtime error is raised, a log entry will state what page count is needed for the operation to succeed.
    - `privileged`: whether to give the coroutine visibility to the kernel.
    - `isolated`: whether to make the process micro-heap invisible when the coroutine is not running.

#### Service Main Entrypoint (Super-Server API)

[`vk_service`](vk_service.c) starts a coroutine thread connected to an accepted socket, as is done in [`vk_test_echo_service.c`](vk_test_echo_service.c).

```c
#include "vk_service.h"
#include "vk_echo.h"

#include <netinet/in.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	int rc;
	struct vk_server* server_ptr;
	struct vk_pool* pool_ptr;
	struct sockaddr_in address;

	server_ptr = calloc(1, vk_server_alloc_size());
	pool_ptr = calloc(1, vk_pool_alloc_size());

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	vk_server_set_pool(server_ptr, pool_ptr);
	vk_server_set_socket(server_ptr, PF_INET, SOCK_STREAM, 0);
	vk_server_set_address(server_ptr, (struct sockaddr*)&address, sizeof(address));
	vk_server_set_backlog(server_ptr, 128);
	vk_server_set_vk_func(server_ptr, vk_echo);
	vk_server_set_count(server_ptr, 0);
	vk_server_set_privileged(server_ptr, 0);
	vk_server_set_isolated(server_ptr, 1);
	vk_server_set_page_count(server_ptr, 25);
	vk_server_set_msg(server_ptr, NULL);
	rc = vk_server_init(server_ptr);
	if (rc == -1) {
		return 1;
	}

	return 0;
}
```

#### Redis RESP Hello World

[`vk_redis.c`](vk_redis.c) implements a basic Redis RESP service with a struct-based query interface and responder coroutine.
It parses synchronous, non-stream RESP array commands like `PING` and responds with `+PONG\r\n`.
See [`vk_test_redis_cli.c`](vk_test_redis_cli.c) for a CLI example and [`vk_test_redis_service.c`](vk_test_redis_service.c) for a server example.

To try it out:

```sh
bmake vk_test_redis_service
./vk_test_redis_service &
printf '*1\r\n$4\r\nPING\r\n' | nc localhost 6379
```

##### Service Listeners
A service listener blocks on a `struct vk_fd::fd_type` of `VK_FD_TYPE_SOCKET_LISTEN`, which does an underlying `accept()` call, but fills the `struct vk_vectoring` buffer with `struct vk_accepted` objects with connection information.

`struct vk_accepted`: accepted connection object
- [`vk_accepted.h`](vk_accepted.h)
- [`vk_accepted_s.h`](vk_accepted_s.h)
- [`vk_accepted.c`](vk_accepted.c)

From that, a new micro-process with a new coroutine thread for a protocol handler are initialized with a `struct vk_pipe` pair connecting the thread's default socket to the accepted socket.

##### Platform Service Listener
`struct vk_service`:
- [`vk_service.h`](vk_service.h)
- [`vk_service_s.h`](vk_service_s.h)
- [`vk_service.c`](vk_service.c):
    - `vk_service_listener()`: platform provided service listener

Since forking a new micro-process is a privileged activity, the platform provides a service listener that already implements this.

##### Platform Service Entrypoint

`struct vk_server`: state of the server connection, and its configuration
- [`vk_server.h`](vk_server.h)
- [`vk_server_s.h`](vk_server_s.h)
- [`vk_server.c`](vk_server.c):
    - `vk_server_init(server)`: initialize a server configured with:
        - `vk_server_set_socket(server, domain, type, protocol)`: args for `socket()` call
        - `vk_server_set_address(server, address, address_len)`: args for `bind()` call
        - `vk_server_set_backlog(backlog)`: arg for `listen()` call
        - `vk_server_set_vk_func(func)`: to set the protocol handling coroutine thread function
        - `vk_server_set_count(count)`: if > 0, initializes a pool of coroutines of the specified count, falling back to `mmap()`, the normal path, when exceeded, which is still very fast
        - `vk_server_set_privileged(bool)`: whether the coroutine can access the virtual kernel
        - `vk_server_set_isolated(bool)`: whether to mask out the coroutine's memory while it is not running
        - `vk_server_set_page_count(count)`: the number of 4096 byte pages to allocate for the micro-process heap for each connection
        - `vk_server_set_msg(ptr)`: pointer to high-level configuration
    - `vk_server_socket_listen()|vk_server_listen()`: used by `vk_service_listener()` to create a listening socket:
        1. takes an existing `struct vk_socket`
        2. creates a server listening socket
        3. replaces the socket's read pipe with the listening socket

### Interfaces

- [`mnvkd` Threading Interface](threads.md)
- [`mnvkd` Object Interface](objects.md)

### Design

- [`mnvkd` Design](design.md)

## Service Development

### Adding a New Service

1. **Create the coroutine**
   - Add `vk_<name>.c` and `vk_<name>.h` following the pattern in
     [`vk_redis.c`](vk_redis.c).
   - Each service exposes `void vk_<name>(struct vk_thread *that)` and uses
     `vk_raise()` for error exits with a `vk_finally()` block for cleanup.

2. **Provide a server harness**
   - Add `vk_test_<name>_service.c` that configures a `struct vk_server` and
     calls `vk_service_listener()` as shown in
     [`vk_test_redis_service.c`](vk_test_redis_service.c).

3. **Wire into the build system**
   - **BSD Make**: add the new test target to `all` and define a build rule:
     `vk_test_<name>_service: vk_test_<name>_service.c vk_<name>.c vk.a`.
   - **CMake**: `add_executable(vk_test_<name>_service vk_test_<name>_service.c vk_<name>.c)`
     followed by `target_link_libraries(vk_test_<name>_service vk)`.

### Testing Expectations

- Build the service with `./debug.sh bmake vk_test_<name>_service`.
- Run the full suite with `./debug.sh bmake test` before committing.
- For interactive checks, run the produced binary and send protocol-specific
  input (e.g., `printf '*1\r\n$4\r\nPING\r\n' | nc localhost 6379` for the
  Redis example).
