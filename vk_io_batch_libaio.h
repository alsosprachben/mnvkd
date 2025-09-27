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

int vk_io_batch_libaio_stage_poll(struct vk_io_libaio_batch* batch,
                                  struct vk_fd* fd_ptr);

int vk_io_batch_libaio_stage_rw(struct vk_io_libaio_batch* batch,
                                struct vk_fd* fd_ptr,
                                struct vk_io_op* op_ptr);

int vk_io_batch_libaio_submit(struct vk_io_libaio_batch* batch,
                              struct vk_fd_table* fd_table_ptr,
                              struct vk_kern* kern_ptr,
                              size_t* submitted_out,
                              int* submit_err_out);

void vk_io_batch_libaio_post_submit(struct vk_io_libaio_batch* batch,
                                    size_t submitted,
                                    int submit_err,
                                    struct vk_fd_table* fd_table_ptr,
                                    int* disable_driver_out);

int vk_io_batch_libaio_reap(struct vk_io_libaio_batch* batch,
                            struct vk_fd_table* fd_table_ptr,
                            struct vk_kern* kern_ptr,
                            size_t submitted);

int vk_io_batch_libaio_had_poll_ops(const struct vk_io_libaio_batch* batch);
size_t vk_io_batch_libaio_count(const struct vk_io_libaio_batch* batch);

#endif /* VK_IO_BATCH_LIBAIO_H */
