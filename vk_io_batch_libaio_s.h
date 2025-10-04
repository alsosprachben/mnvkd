#ifndef VK_IO_BATCH_LIBAIO_S_H
#define VK_IO_BATCH_LIBAIO_S_H

#include <stddef.h>

#include "vk_io_batch_iface.h"

struct iocb;
struct io_event;
struct vk_fd_aio_meta;

struct vk_io_libaio_batch {
    struct vk_io_batch base;
    struct iocb** submit_list;
    struct vk_fd_aio_meta** meta_list;
    struct io_event* events;
    size_t submit_count;
    size_t max_entries;
    int had_poll_ops;
};

#endif /* VK_IO_BATCH_LIBAIO_S_H */
