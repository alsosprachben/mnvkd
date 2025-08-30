# vk_* Lifecycle and Shutdown

This document traces how core objects in `mnvkd` clean up resources during shutdown.

## Kernel (`vk_kern`)

The kernel exposes a shutdown flag via `vk_kern_set_shutdown_requested` so higher layers can request the event loop to exit【F:vk_kern.c†L52-L56】. The main loop continues polling while there are runnable or blocked processes and dumps the final state once none remain【F:vk_kern.c†L646-L672】.

## Process (`vk_proc`)

Processes own a private heap and a list of file descriptors. Prior to each poll cycle, `vk_proc_prepoll` walks the descriptor list, deregisters closed entries and deallocates them from the process【F:vk_proc.c†L322-L340】. When a process becomes a zombie, the kernel frees its descriptors, unmaps or exits the heap, and returns the process to the pool【F:vk_kern.c†L571-L592】. The heap mapping itself is released by `vk_proc_free`【F:vk_proc.c†L232-L241】.

## Thread (`vk_thread`)

A thread's coroutine state and socket are allocated on the process's stack. Deinitialization with `vk_deinit` simply pops these structures so a completed coroutine leaves no residual allocations【F:vk_thread.c†L66-L70】.

## File Descriptor (`vk_fd`)

File descriptors record ownership and lifecycle state. Allocation marks them in use and resets bookkeeping fields【F:vk_fd.c†L17-L29】, while `vk_fd_free` clears the flags when the descriptor is released【F:vk_fd.c†L32-L37】. When a process closes a descriptor, `vk_proc_deallocate_fd` removes it from the process list so the kernel can stop tracking it【F:vk_proc.c†L89-L96】.

## Summary

By cooperatively flagging closure and allowing the kernel to flush zombies from its pools, `mnvkd` ensures that kernels, processes, threads and file descriptors relinquish all resources as the system shuts down.
