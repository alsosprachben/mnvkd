# M:N Virtual Kernel Daemon

## Synopsis

### `mnvkd` is a C service stack, comprised of:
1. A soft-real-time, actor-based virtual kernel and threaded coroutine framework,
2. A "Super-Server" framework and protocol suite, and
3. A cloud and edge application server and runtime library.

#### (#1) `mnvkd` is an M:N actor-based C alternative to Erlang and Golang:
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

#### (#2) `mnvkd` is a "Super-Server" like `systemd`, `launchd`, and `inetd`:
1. but instead of being process-based, is event-based, while
2. preserving the simple interface of `inetd`,
3. preserving the privilege separation of `inetd` and `systemd`, and
4. preserving an unfettered C environment.
5. Designed for implementing distributed service protocols:
   1. HTTP servers and clients
   2. DNS
   3. Storage Services, like S3, Redis RESP, ElasticSearch, etc.

#### (#3) `mnvkd` seeks to provide a `Web-interoperable Runtime` library for cloud and edge services:
1. but instead of being JavaScript, it is C, where
2. interfaces are C-idiomatic variants of the [WinterCG](https://wintercg.org/) standard interfaces, with
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
1. The [stackless coroutine system](interfaces.md#coroutines) is derived from [Simon Tatham's coroutines](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html) but with a novel enhancement.
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
6which is what makes in-process memory protection actually feasible.

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

See the Standard I/O blocking operations for writing HTTP response line and headers in the response handler.

Chunked encoding is handled by `vk_rfc.h` chunk operations:
##### logical reads and writes between `struct vk_rfcchunk`
- `vk_readrfcchunk(rc_arg, chunk_arg)`: read into `struct vk_rfcchunk`
- `vk_writerfcchunk(chunk_arg)`: write from `struct vk_rfcchunk`

##### physical protocol reads and writes between `struct vk_rfcchunk`
- `vk_writerfcchunk_proto(rc_arg, chunk_arg)`: write an HTTP chunk from `struct vk_rfcchunk`
- `vk_writerfcchunkend_proto()`: write the terminating HTTP chunk
- `vk_readrfcchunk_proto(rc_arg, chunk_arg)`: read HTTP chunk into `struct vk_rfcchunk`

The interface for `struct vk_rfcchunk` can be found in `vk_rfc.h`, but it is only used by these blocking operations, so it can typically be ignored.

##### `vk_fetch`

From `vk_fetch.c`, a Fetch API implementation. Not completed yet. 

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

### Writing a Service

See `vk_echo.c` or `vk_http11.c`. The I/O API is in `vk_thread_io.h`, and utilities for implementing some RFCs are in `vk_rfc.h`.

See `vk_test_*_cli.c` for a regular CLI command hooked to stdin and stdout. See `vk_test_*_service.c` for a service hooked into a network server.

The full set of thread APIs are in the `vk_thread*.h` files. How the blocking ops work is explained below.

The threads reside in processes:
- A "privileged" process has access to the kernel state, otherwise the kernel is masked out by `mprotect()`.
- An "isolated" process has its state masked out by `mprotect()` when it is not running.

That is, for each process:
- set `privileged=0`, `isolated=1` for full isolation, for most security, or
- set `privileged=1`, `isolated=0` for no isolation, for most throughput.

When "privileged" is not set, the entire kernel space is mapped out. On Linux, huge pages are used to speed this up 100x. On other systems, this can be very slow. Some systems can be configured with dynamic page sizes, which could help.

### Interfaces

See [M:N Virtual Kernel Daemon Interfaces](interfaces.md).

### Design

See [M:N Virtual Kernel Daemon Design](design.md).