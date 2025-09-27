#include "vk_fd_table.h"
#include "vk_fd_table_s.h"
#include "vk_io_batch_libaio.h"
#include "vk_io_batch_libaio_s.h"

#if VK_USE_GETEVENTS

#include "vk_debug.h"
#include "vk_fd.h"
#include "vk_io_exec.h"
#include "vk_io_op.h"
#include "vk_io_op_s.h"
#include "vk_thread_io.h"
#include "vk_kern.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>

void
vk_io_batch_libaio_init(struct vk_io_libaio_batch* batch,
                        struct iocb** submit_list,
                        struct vk_fd_aio_meta** meta_list,
                        struct io_event* events,
                        size_t max_entries)
{
    if (!batch) return;
    batch->submit_list = submit_list;
    batch->meta_list = meta_list;
    batch->events = events;
    batch->submit_count = 0;
    batch->max_entries = max_entries;
    batch->had_poll_ops = 0;
}

int
vk_io_batch_libaio_stage_poll(struct vk_io_libaio_batch* batch,
                              struct vk_fd* fd_ptr)
{
    if (!batch || !fd_ptr) return -1;
    if (batch->submit_count >= batch->max_entries) {
        return -1;
    }

    struct iocb* poll_iocb = &fd_ptr->aio.poll_iocb;
    struct vk_fd_aio_meta* meta = &fd_ptr->aio.poll_meta;

    memset(poll_iocb, 0, sizeof(*poll_iocb));
    fd_ptr->aio.poll_data.events = (uint64_t)(uint32_t)fd_ptr->ioft_pre.event.events;
    fd_ptr->aio.poll_data.resfd = 0;
    poll_iocb->aio_data = (uint64_t)(uintptr_t)meta;
    poll_iocb->aio_fildes = fd_ptr->fd;
    poll_iocb->aio_lio_opcode = IOCB_CMD_POLL;
    poll_iocb->aio_buf = (uint64_t)(uintptr_t)&fd_ptr->aio.poll_data;

    meta->kind = VK_FD_AIO_META_POLL;
    meta->fd = fd_ptr;
    meta->op = NULL;

    fd_ptr->ioft_post = fd_ptr->ioft_pre;

    DBG("libaio stage driver=io_submit fd=%d poll events=0x%llx\n",
        fd_ptr->fd, (unsigned long long)fd_ptr->aio.poll_data.events);

    batch->submit_list[batch->submit_count] = poll_iocb;
    batch->meta_list[batch->submit_count] = meta;
    batch->submit_count++;
    batch->had_poll_ops = 1;

    return 0;
}

int
vk_io_batch_libaio_stage_rw(struct vk_io_libaio_batch* batch,
                            struct vk_fd* fd_ptr,
                            struct vk_io_op* op_ptr)
{
    if (!batch || !fd_ptr || !op_ptr) return -1;
    if (batch->submit_count >= batch->max_entries) {
        return -1;
    }

    enum VK_IO_OP_KIND kind = vk_io_op_get_kind(op_ptr);
    struct iocb* rw_iocb = &fd_ptr->aio.rw_iocb;
    struct vk_fd_aio_meta* meta = &fd_ptr->aio.rw_meta;
    const char* kind_str;
    size_t iovcnt = op_ptr->iov[1].iov_len > 0 ? 2 : 1;

    memset(rw_iocb, 0, sizeof(*rw_iocb));
    rw_iocb->aio_data = (uint64_t)(uintptr_t)meta;
    rw_iocb->aio_fildes = fd_ptr->fd;
    rw_iocb->aio_lio_opcode = (kind == VK_IO_READ) ? IOCB_CMD_PREADV : IOCB_CMD_PWRITEV;
    rw_iocb->aio_buf = (uint64_t)(uintptr_t)op_ptr->iov;
    rw_iocb->aio_nbytes = iovcnt;
    rw_iocb->aio_offset = 0;
    rw_iocb->aio_rw_flags = 0;

    meta->kind = (kind == VK_IO_READ) ? VK_FD_AIO_META_READ : VK_FD_AIO_META_WRITE;
    meta->fd = fd_ptr;
    meta->op = op_ptr;

    op_ptr->state = VK_IO_OP_SUBMITTED;
    op_ptr->err = 0;
    op_ptr->res = 0;

    batch->submit_list[batch->submit_count] = rw_iocb;
    batch->meta_list[batch->submit_count] = meta;
    batch->submit_count++;

    kind_str = (kind == VK_IO_READ) ? "READ" : "WRITE";
    DBG("libaio stage driver=io_submit fd=%d kind=%s len=%zu iovcnt=%zu\n",
        fd_ptr->fd, kind_str, op_ptr->len, iovcnt);

    return 0;
}

int
vk_io_batch_libaio_submit(struct vk_io_libaio_batch* batch,
                          struct vk_fd_table* fd_table_ptr,
                          struct vk_kern* kern_ptr,
                          size_t* submitted_out,
                          int* submit_err_out)
{
    if (!batch || !fd_table_ptr) {
        return -1;
    }

    if (submitted_out) *submitted_out = 0;
    if (submit_err_out) *submit_err_out = 0;

    if (batch->submit_count == 0) {
        return 0;
    }

    vk_kern_receive_signal(kern_ptr);
    DBG("io_submit driver=io_submit(ctx=%p, count=%zu)",
        &fd_table_ptr->aio_ctx, batch->submit_count);
    long rc = syscall(SYS_io_submit,
                      fd_table_ptr->aio_ctx,
                      (long)batch->submit_count,
                      batch->submit_list);
    DBG(" = %li errno=%d\n", rc, rc == -1 ? errno : 0);
    vk_kern_receive_signal(kern_ptr);

    size_t submitted = 0;
    int submit_err = 0;
    if (rc >= 0) {
        submitted = (size_t)rc;
    } else {
        submit_err = errno;
        if (submit_err == EINTR) {
            submitted = 0;
            submit_err = 0;
        } else if (submit_err != EAGAIN && submit_err != EINVAL && submit_err != EOPNOTSUPP) {
            PERROR("io_submit");
            if (submit_err_out) *submit_err_out = submit_err;
            return -1;
        }
    }

    if (submitted_out) *submitted_out = submitted;
    if (submit_err_out) *submit_err_out = submit_err;
    return 0;
}

void
vk_io_batch_libaio_post_submit(struct vk_io_libaio_batch* batch,
                               size_t submitted,
                               int submit_err,
                               struct vk_fd_table* fd_table_ptr,
                               int* disable_driver_out)
{
    if (!batch || !fd_table_ptr) return;
    int disable_driver = 0;

    for (size_t i = submitted; i < batch->submit_count; ++i) {
        struct vk_fd_aio_meta* meta = batch->meta_list[i];
        if (!meta) {
            continue;
        }

        if (meta->kind == VK_FD_AIO_META_READ || meta->kind == VK_FD_AIO_META_WRITE) {
            if (meta->op) {
                meta->op->err = 0;
                meta->op->res = 0;
                meta->op->state = (submit_err == EINVAL || submit_err == EOPNOTSUPP)
                                      ? VK_IO_OP_PENDING
                                      : VK_IO_OP_STAGED;
                meta->op = NULL;
            }
            if (submit_err == EINVAL || submit_err == EOPNOTSUPP) {
                vk_fd_disable_cap(meta->fd, VK_FD_CAP_AIO_RW);
            }
        } else if (meta->kind == VK_FD_AIO_META_POLL) {
            if (submit_err == EINVAL || submit_err == EOPNOTSUPP) {
                disable_driver = 1;
            }
        }

        vk_fd_table_enqueue_dirty(fd_table_ptr, meta->fd);
    }

    if (disable_driver_out) {
        *disable_driver_out = disable_driver;
    }
}

static int
vk_io_batch_libaio_finalize_event(struct vk_fd_table* fd_table_ptr,
                                  struct vk_fd_aio_meta* meta,
                                  struct io_event* event)
{
    if (!meta || !meta->fd) {
        return 0;
    }

    struct vk_fd* event_fd = meta->fd;

    if (meta->kind == VK_FD_AIO_META_POLL) {
        event_fd->ioft_post = event_fd->ioft_pre;
        event_fd->ioft_post.event.revents = (short)event->res;
        vk_fd_table_process_fd(fd_table_ptr, event_fd);
        vk_fd_table_clean_fd(fd_table_ptr, event_fd);
        return 0;
    }

    struct vk_io_op* op_ptr = meta->op;
    meta->op = NULL;
    if (!op_ptr) {
        return 0;
    }

    ssize_t res = (ssize_t)event->res;
    if (res < 0) {
        int err = (int)(-res);
        if (err == EINVAL || err == EOPNOTSUPP) {
            vk_fd_disable_cap(event_fd, VK_FD_CAP_AIO_RW);
            vk_io_op_set_state(op_ptr, VK_IO_OP_PENDING);
            op_ptr->err = 0;
            op_ptr->res = 0;
            vk_fd_table_enqueue_dirty(fd_table_ptr, event_fd);
            return 0;
        }
        errno = err;
        vk_io_exec_finalize_rw(op_ptr, -1);
    } else {
        vk_io_exec_finalize_rw(op_ptr, res);
    }

    vk_thread_io_complete_op(&event_fd->socket, vk_fd_get_io_queue(event_fd), op_ptr);
    vk_fd_table_clean_fd(fd_table_ptr, event_fd);
    return 0;
}

int
vk_io_batch_libaio_reap(struct vk_io_libaio_batch* batch,
                        struct vk_fd_table* fd_table_ptr,
                        struct vk_kern* kern_ptr,
                        size_t submitted)
{
    if (!batch || !fd_table_ptr || submitted == 0) {
        return 0;
    }

    struct timespec timeout;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    vk_kern_receive_signal(kern_ptr);
    DBG("io_getevents driver=io_submit(ctx=%p, min=1, max=%zu, timeout=1s)",
        &fd_table_ptr->aio_ctx, submitted);
    long rc = syscall(SYS_io_getevents,
                      fd_table_ptr->aio_ctx,
                      1,
                      (long)submitted,
                      batch->events,
                      &timeout);
    DBG(" = %li\n", rc);
    vk_kern_receive_signal(kern_ptr);

    if (rc == -1) {
        if (errno == EINTR) {
            return 0;
        }
        PERROR("io_getevents");
        return -1;
    }

    for (size_t i = 0; i < (size_t)rc; ++i) {
        struct io_event* event = &batch->events[i];
        struct vk_fd_aio_meta* meta = (struct vk_fd_aio_meta*)(uintptr_t)event->data;
        vk_io_batch_libaio_finalize_event(fd_table_ptr, meta, event);
    }

    return 0;
}

int
vk_io_batch_libaio_had_poll_ops(const struct vk_io_libaio_batch* batch)
{
    if (!batch) return 0;
    return batch->had_poll_ops;
}

size_t
vk_io_batch_libaio_count(const struct vk_io_libaio_batch* batch)
{
    if (!batch) return 0;
    return batch->submit_count;
}

#endif /* VK_USE_GETEVENTS */
