#ifndef VK_IO_BATCH_LIBAIO_H
#define VK_IO_BATCH_LIBAIO_H

#include <stddef.h>

struct iocb;
struct io_event;
struct vk_fd;
struct vk_fd_aio_meta;
struct vk_fd_table;
struct vk_io_op;
struct vk_io_libaio_batch;
struct vk_kern;

void vk_io_batch_libaio_init(struct vk_io_libaio_batch* batch,
                             struct iocb** submit_list,
                             struct vk_fd_aio_meta** meta_list,
                             struct io_event* events,
                             size_t max_entries);

#endif /* VK_IO_BATCH_LIBAIO_H */
