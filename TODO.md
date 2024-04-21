#TODO

- Dockerfile volume builder
- kqueue poller
- HTTP servlet that loads static files into memory, in a reactive sense.
- HTTP client
- libtls coroutine
  - server coroutine
    - `vk_socket_peak()` for HelloClient header routing
  - client coroutine (with mTLS support)
- `vk_socket_open()` API
- `struct vk_filemap` object, for file update events (`fstat()`, or get data from event) 
- `vk_alloc_socket()` uses vk_alloc() to get a socket.
- `vk_socket_connect()` API
- `struct vk_timer_table` object, for kqueue timer events