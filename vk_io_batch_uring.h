#ifndef VK_IO_BATCH_URING_H
#define VK_IO_BATCH_URING_H

#include <stddef.h>

#include "vk_io_batch_iface.h"
#include "vk_io_batch_uring_s.h"

struct vk_io_uring_batch {
    struct vk_io_batch base;
    struct vk_io_uring_driver* driver;
    struct vk_fd_uring_meta** meta_list;
    size_t max_entries;
    size_t staged_sqes;
    size_t staged_poll;
    size_t staged_rw;
};

void vk_io_batch_uring_init(struct vk_io_uring_batch* batch,
                            struct vk_io_uring_driver* driver,
                            struct vk_fd_uring_meta** meta_list,
                            size_t max_entries);

#endif /* VK_IO_BATCH_URING_H */
