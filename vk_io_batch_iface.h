#ifndef VK_IO_BATCH_IFACE_H
#define VK_IO_BATCH_IFACE_H

#include <stddef.h>

struct vk_fd;
struct vk_fd_table;
struct vk_kern;
struct vk_io_op;

struct vk_io_batch;

struct vk_io_batch_ops {
    void (*reset)(struct vk_io_batch* batch);
    size_t (*count)(const struct vk_io_batch* batch);
    int (*stage_poll)(struct vk_io_batch* batch, struct vk_fd* fd_ptr);
    int (*stage_rw)(struct vk_io_batch* batch, struct vk_fd* fd_ptr, struct vk_io_op* op_ptr);
    int (*submit)(struct vk_io_batch* batch,
                  struct vk_fd_table* fd_table_ptr,
                  struct vk_kern* kern_ptr,
                  size_t* submitted_out,
                  int* submit_err_out);
    void (*post_submit)(struct vk_io_batch* batch,
                        size_t submitted,
                        int submit_err,
                        struct vk_fd_table* fd_table_ptr,
                        int* disable_driver_out);
    int (*reap)(struct vk_io_batch* batch,
                struct vk_fd_table* fd_table_ptr,
                struct vk_kern* kern_ptr,
                size_t submitted);
    int (*had_poll_ops)(const struct vk_io_batch* batch);
};

struct vk_io_batch {
    const struct vk_io_batch_ops* ops;
};

static inline void
vk_io_batch_init(struct vk_io_batch* batch, const struct vk_io_batch_ops* ops)
{
    if (batch) {
        batch->ops = ops;
    }
}

static inline void
vk_io_batch_reset(struct vk_io_batch* batch)
{
    if (batch && batch->ops && batch->ops->reset) {
        batch->ops->reset(batch);
    }
}

static inline size_t
vk_io_batch_count(const struct vk_io_batch* batch)
{
    if (!batch || !batch->ops || !batch->ops->count) {
        return 0;
    }
    return batch->ops->count(batch);
}

static inline int
vk_io_batch_stage_poll(struct vk_io_batch* batch, struct vk_fd* fd_ptr)
{
    if (!batch || !batch->ops || !batch->ops->stage_poll) {
        return -1;
    }
    return batch->ops->stage_poll(batch, fd_ptr);
}

static inline int
vk_io_batch_stage_rw(struct vk_io_batch* batch, struct vk_fd* fd_ptr, struct vk_io_op* op_ptr)
{
    if (!batch || !batch->ops || !batch->ops->stage_rw) {
        return -1;
    }
    return batch->ops->stage_rw(batch, fd_ptr, op_ptr);
}

static inline int
vk_io_batch_submit(struct vk_io_batch* batch,
                   struct vk_fd_table* fd_table_ptr,
                   struct vk_kern* kern_ptr,
                   size_t* submitted_out,
                   int* submit_err_out)
{
    if (!batch || !batch->ops || !batch->ops->submit) {
        return -1;
    }
    return batch->ops->submit(batch, fd_table_ptr, kern_ptr, submitted_out, submit_err_out);
}

static inline void
vk_io_batch_post_submit(struct vk_io_batch* batch,
                        size_t submitted,
                        int submit_err,
                        struct vk_fd_table* fd_table_ptr,
                        int* disable_driver_out)
{
    if (batch && batch->ops && batch->ops->post_submit) {
        batch->ops->post_submit(batch, submitted, submit_err, fd_table_ptr, disable_driver_out);
    }
}

static inline int
vk_io_batch_reap(struct vk_io_batch* batch,
                 struct vk_fd_table* fd_table_ptr,
                 struct vk_kern* kern_ptr,
                 size_t submitted)
{
    if (!batch || !batch->ops || !batch->ops->reap) {
        return 0;
    }
    return batch->ops->reap(batch, fd_table_ptr, kern_ptr, submitted);
}

static inline int
vk_io_batch_had_poll_ops(const struct vk_io_batch* batch)
{
    if (!batch || !batch->ops || !batch->ops->had_poll_ops) {
        return 0;
    }
    return batch->ops->had_poll_ops(batch);
}

#endif /* VK_IO_BATCH_IFACE_H */
