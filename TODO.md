# TODO

- Integrate vk_pool.
  - coroutine deinit function as init callback (dependency injection)
  - move zombie killing to kernel
- Remove vk_kern from vk_proc:
  - Move kernel side effects to kernel:
    - Make queue flush to kernel a flag that the kernel reads after proc exec.
    - Move execution signal setup to kernel
- Dockerfile volume builder
- kqueue poller
- HTTP servlet that loads static files into memory
- HTTP client
- libtls coroutine
  - server coroutine
    - vk_peak() for HelloClient header routing
  - client coroutine (with mTLS support)
