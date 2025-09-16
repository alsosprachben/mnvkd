#ifndef VK_IO_BATCH_SYNC_H
#define VK_IO_BATCH_SYNC_H

#include <stddef.h>
#include <poll.h>

struct vk_io_queue;
struct vk_io_op;

struct vk_io_fd_stream {
    int fd;
    struct vk_io_queue* q;
};

/*
 * Run a minimal synchronous batch across multiple fd streams.
 * For each stream, pop at most one op, issue nonblocking readv/writev,
 * and classify outcome:
 *  - completed[] gets op pointers with op->state set to DONE/ERROR/NEEDS_POLL.
 *  - needs_poll[] gets (fd, events) for ops that require readiness.
 * Returns the number of ops processed (== *completed_n on success).
 */
size_t vk_io_batch_sync_run(const struct vk_io_fd_stream* streams,
                            size_t nstreams,
                            struct vk_io_op** completed,
                            size_t* completed_n,
                            struct pollfd* needs_poll,
                            size_t* needs_poll_n);

#endif /* VK_IO_BATCH_SYNC_H */

