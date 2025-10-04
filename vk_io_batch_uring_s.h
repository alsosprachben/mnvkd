#ifndef VK_IO_BATCH_URING_S_H
#define VK_IO_BATCH_URING_S_H

#ifndef VK_USE_IO_URING
#if defined(__linux__)
#define VK_USE_IO_URING 1
#else
#define VK_USE_IO_URING 0
#endif
#endif

#include <stdint.h>

#include <liburing.h>

struct vk_fd;
struct vk_io_op;

enum vk_fd_uring_meta_kind {
    VK_FD_URING_META_POLL = 0,
    VK_FD_URING_META_READ,
    VK_FD_URING_META_WRITE,
};

struct vk_fd_uring_meta {
    enum vk_fd_uring_meta_kind kind;
    struct vk_fd* fd;
    struct vk_io_op* op;
};

struct vk_fd_uring_state {
    unsigned registered_file : 1;
    unsigned registered_buffer : 1;
    unsigned poll_armed : 1;
    unsigned in_flight_rw : 1;
    unsigned reserved : 4;
    uint32_t file_slot;
    uint32_t buf_slot;
    void* pending_rw;
    struct vk_fd_uring_meta poll_meta;
    struct vk_fd_uring_meta rw_meta;
};

struct vk_io_uring_driver {
    struct io_uring ring;
    unsigned queue_depth;
    unsigned pending_sqes;
    unsigned pending_poll;
    unsigned pending_rw;
    struct __kernel_timespec wait_timeout;
};

#endif /* VK_IO_BATCH_URING_S_H */
