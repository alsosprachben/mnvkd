# vk_* Cleanup and Loop Termination

This document summarizes how core objects release resources and exit their control loops.

## Kernel (`vk_kern`)

`vk_kern_loop` repeatedly prepares, polls, and posts events while there are runnable or blocked processes. The loop stops once both queues are empty and the kernel dumps its final state【F:vk_kern.c†L646-L672】【F:vk_kern.c†L469-L472】.

New poll events are dispatched by draining fresh file descriptors and runnable processes; each inner loop terminates when its queue is empty【F:vk_kern.c†L615-L629】.

If a process becomes a zombie—meaning it has no runnable or blocked threads—the kernel frees its descriptors, releases the heap, and removes the process from tracking【F:vk_kern.c†L571-L592】【F:vk_proc.c†L68】.

## Process (`vk_proc`)

During `vk_proc_prepoll`, the process iterates over its file descriptors, enqueuing closed ones for cleanup and deallocating them before registering blocked sockets with the poller【F:vk_proc.c†L313-L352】. After polling, `vk_proc_postpoll` walks the blocked socket list again to apply readiness events from the kernel’s FD table【F:vk_proc.c†L364-L405】. These loops end once all descriptors or sockets have been processed.

A process is considered a zombie when both its runnable and blocked counters drop to zero【F:vk_proc.c†L62-L68】.

## Thread (`vk_thread`)

When a coroutine completes, `vk_deinit` pops the coroutine’s state and socket off the process stack so no allocations linger beyond the thread’s lifetime【F:vk_thread.c†L66-L70】.

## File Descriptor (`vk_fd`)

Allocating a descriptor records its owning process and resets its bookkeeping fields, while `vk_fd_free` clears them on release【F:vk_fd.c†L17-L38】. Closed descriptors are removed from a process by `vk_proc_deallocate_fd`, and `vk_fd_table_clean_fd` marks descriptors closed once both sides have shut down, dropping them from poller queues【F:vk_proc.c†L89-L96】【F:vk_fd_table.c†L299-L331】.

## OS File Descriptor

Socket close operations invoke `vk_socket_handle_tx_close` or `vk_socket_handle_rx_close`, which call `vk_vectoring_close` to perform the actual `close(2)` and mark the I/O rings shut【F:vk_socket.c†L450-L505】【F:vk_vectoring.c†L550-L566】.

