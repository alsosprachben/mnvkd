#include "vk_io_batch_uring.h"

#if VK_USE_IO_URING

#include "vk_debug.h"
#include "vk_fd.h"
#include "vk_fd_table.h"
#include "vk_fd_table_s.h"
#include "vk_io_exec.h"
#include "vk_io_op.h"
#include "vk_io_op_s.h"
#include "vk_thread_io.h"
#include "vk_kern.h"

#include <errno.h>
#include <string.h>

static inline struct vk_io_uring_batch*
uring_from_base(struct vk_io_batch* base)
{
    return base ? (struct vk_io_uring_batch*)base : NULL;
}

static inline const struct vk_io_uring_batch*
uring_from_base_const(const struct vk_io_batch* base)
{
    return base ? (const struct vk_io_uring_batch*)base : NULL;
}

static void
vk_io_batch_uring_reset_impl(struct vk_io_batch* base)
{
    struct vk_io_uring_batch* batch = uring_from_base(base);
    if (!batch) {
        return;
    }
    batch->staged_sqes = 0;
    batch->staged_poll = 0;
    batch->staged_rw = 0;
    if (batch->meta_list && batch->max_entries > 0) {
        memset(batch->meta_list, 0, sizeof(*batch->meta_list) * batch->max_entries);
    }
}

static size_t
vk_io_batch_uring_count_impl(const struct vk_io_batch* base)
{
    const struct vk_io_uring_batch* batch = uring_from_base_const(base);
    return batch ? batch->staged_sqes : 0;
}

static int
vk_io_batch_uring_stage_poll_impl(struct vk_io_batch* base, struct vk_fd* fd_ptr)
{
    struct vk_io_uring_batch* batch = uring_from_base(base);
    if (!batch || !batch->driver || !fd_ptr) {
        return -1;
    }
    if (batch->staged_sqes >= batch->max_entries) {
        return -1;
    }

    struct vk_fd_uring_state* uring_state = &fd_ptr->uring;
    if (uring_state->poll_armed) {
        return 0;
    }

    struct io_uring_sqe* sqe = io_uring_get_sqe(&batch->driver->ring);
    if (!sqe) {
        return -1;
    }

    struct vk_fd_uring_meta* meta = &uring_state->poll_meta;
    meta->kind = VK_FD_URING_META_POLL;
    meta->fd = fd_ptr;
    meta->op = NULL;

    fd_ptr->ioft_post = fd_ptr->ioft_pre;
    io_uring_prep_poll_add(sqe, fd_ptr->fd, (unsigned)fd_ptr->ioft_pre.event.events);
    io_uring_sqe_set_data(sqe, meta);

    uring_state->poll_armed = 1;

    batch->meta_list[batch->staged_sqes] = meta;
    batch->staged_sqes++;
    batch->staged_poll++;

    DBG("io_uring stage fd=%d poll events=0x%x\n", fd_ptr->fd, (unsigned)fd_ptr->ioft_pre.event.events);
    return 0;
}

static int
vk_io_batch_uring_stage_rw_impl(struct vk_io_batch* base,
                                struct vk_fd* fd_ptr,
                                struct vk_io_op* op_ptr)
{
    struct vk_io_uring_batch* batch = uring_from_base(base);
    if (!batch || !batch->driver || !fd_ptr || !op_ptr) {
        return -1;
    }
    if (batch->staged_sqes >= batch->max_entries) {
        return -1;
    }

    struct vk_fd_uring_state* uring_state = &fd_ptr->uring;
    if (uring_state->in_flight_rw) {
        return 0;
    }

    enum VK_IO_OP_KIND kind = vk_io_op_get_kind(op_ptr);
    if (kind != VK_IO_READ && kind != VK_IO_WRITE) {
        return -1;
    }

    struct io_uring_sqe* sqe = io_uring_get_sqe(&batch->driver->ring);
    if (!sqe) {
        return -1;
    }

    size_t iovcnt = op_ptr->iov[1].iov_len > 0 ? 2 : 1;
    if (kind == VK_IO_READ) {
        io_uring_prep_readv(sqe, fd_ptr->fd, op_ptr->iov, (unsigned)iovcnt, 0);
    } else {
        io_uring_prep_writev(sqe, fd_ptr->fd, op_ptr->iov, (unsigned)iovcnt, 0);
    }

    struct vk_fd_uring_meta* meta = &uring_state->rw_meta;
    meta->kind = (kind == VK_IO_READ) ? VK_FD_URING_META_READ : VK_FD_URING_META_WRITE;
    meta->fd = fd_ptr;
    meta->op = op_ptr;

    io_uring_sqe_set_data(sqe, meta);

    uring_state->in_flight_rw = 1;
    uring_state->pending_rw = op_ptr;

    op_ptr->state = VK_IO_OP_SUBMITTED;
    op_ptr->err = 0;
    op_ptr->res = 0;

    batch->meta_list[batch->staged_sqes] = meta;
    batch->staged_sqes++;
    batch->staged_rw++;

    DBG("io_uring stage fd=%d kind=%s len=%zu iovcnt=%zu\n",
        fd_ptr->fd, kind == VK_IO_READ ? "READ" : "WRITE", op_ptr->len, iovcnt);
    return 0;
}

static int
vk_io_batch_uring_submit_impl(struct vk_io_batch* base,
                              struct vk_fd_table* fd_table_ptr,
                              struct vk_kern* kern_ptr,
                              size_t* submitted_out,
                              int* submit_err_out)
{
    struct vk_io_uring_batch* batch = uring_from_base(base);
    if (!batch || !batch->driver) {
        return -1;
    }

    if (submitted_out) *submitted_out = 0;
    if (submit_err_out) *submit_err_out = 0;

    if (batch->staged_sqes == 0) {
        return 0;
    }

    vk_kern_receive_signal(kern_ptr);
    int rc = io_uring_submit(&batch->driver->ring);
    vk_kern_receive_signal(kern_ptr);

    size_t submitted = 0;
    int submit_err = 0;
    if (rc >= 0) {
        submitted = (size_t)rc;
    } else {
        submit_err = -rc;
        if (submit_err == EAGAIN || submit_err == EBUSY) {
            int wait_rc = io_uring_submit_and_wait(&batch->driver->ring, 1);
            if (wait_rc >= 0) {
                submitted = (size_t)wait_rc;
                submit_err = 0;
            } else {
                submit_err = -wait_rc;
            }
        } else if (submit_err == EINTR) {
            submit_err = 0;
        }

        if (submit_err != 0 && submit_err != EAGAIN && submit_err != EBUSY &&
            submit_err != EINVAL && submit_err != EOPNOTSUPP) {
            if (submit_err_out) *submit_err_out = submit_err;
            return -1;
        }
    }

    if (submitted > 0) {
        size_t submitted_poll = 0;
        size_t submitted_rw = 0;
        size_t limit = submitted < batch->staged_sqes ? submitted : batch->staged_sqes;
        for (size_t i = 0; i < limit; ++i) {
            struct vk_fd_uring_meta* meta = batch->meta_list[i];
            if (!meta) continue;
            if (meta->kind == VK_FD_URING_META_POLL) {
                submitted_poll++;
            } else {
                submitted_rw++;
            }
        }
        batch->driver->pending_sqes += submitted;
        batch->driver->pending_poll += submitted_poll;
        batch->driver->pending_rw += submitted_rw;
    }

    if (submitted_out) *submitted_out = submitted;
    if (submit_err_out) *submit_err_out = submit_err;
    return 0;
}

static void
vk_io_batch_uring_post_submit_impl(struct vk_io_batch* base,
                                   size_t submitted,
                                   int submit_err,
                                   struct vk_fd_table* fd_table_ptr,
                                   int* disable_driver_out)
{
    struct vk_io_uring_batch* batch = uring_from_base(base);
    if (!batch) {
        return;
    }

    int disable_driver = 0;
    size_t limit = batch->staged_sqes;
    if (submitted > limit) {
        submitted = limit;
    }

    for (size_t i = submitted; i < limit; ++i) {
        struct vk_fd_uring_meta* meta = batch->meta_list[i];
        if (!meta) {
            continue;
        }
        struct vk_fd* fd_ptr = meta->fd;
        if (!fd_ptr) {
            continue;
        }

        if (meta->kind == VK_FD_URING_META_POLL) {
            fd_ptr->uring.poll_armed = 0;
            vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
            if (submit_err == EINVAL || submit_err == EOPNOTSUPP) {
                disable_driver = 1;
            }
        } else {
            struct vk_io_op* op_ptr = meta->op;
            meta->op = NULL;
            fd_ptr->uring.in_flight_rw = 0;
            fd_ptr->uring.pending_rw = NULL;
            if (submit_err == EINVAL || submit_err == EOPNOTSUPP) {
                vk_fd_disable_cap(fd_ptr, VK_FD_CAP_IO_URING);
                disable_driver = 1;
            }
            if (op_ptr) {
                op_ptr->err = 0;
                op_ptr->res = 0;
                op_ptr->state = (submit_err == EINVAL || submit_err == EOPNOTSUPP)
                                    ? VK_IO_OP_PENDING
                                    : VK_IO_OP_STAGED;
            }
            vk_fd_table_enqueue_dirty(fd_table_ptr, fd_ptr);
        }
    }

    if (disable_driver_out) {
        *disable_driver_out = disable_driver;
    }

    vk_io_batch_uring_reset_impl(base);
}

static void
vk_io_batch_uring_finalize_rw(struct vk_fd_table* fd_table_ptr,
                              struct vk_fd* fd_ptr,
                              struct vk_io_op* op_ptr,
                              ssize_t res)
{
    if (res < 0) {
        errno = (int)(-res);
        vk_io_exec_finalize_rw(op_ptr, -1);
    } else {
        vk_io_exec_finalize_rw(op_ptr, res);
    }

    vk_thread_io_complete_op(&fd_ptr->socket, vk_fd_get_io_queue(fd_ptr), op_ptr);
    vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);
}

static void
vk_io_batch_uring_finalize_poll(struct vk_fd_table* fd_table_ptr,
                                struct vk_fd* fd_ptr,
                                unsigned events)
{
	unsigned mask = events;
	if ((int)events < 0) {
		mask = 0;
	}
	fd_ptr->ioft_post = fd_ptr->ioft_pre;
	fd_ptr->ioft_post.event.revents = (short)mask;
	fd_ptr->uring.poll_armed = 0;
	vk_fd_table_process_fd(fd_table_ptr, fd_ptr);
	vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);
}

static int
vk_io_batch_uring_reap_impl(struct vk_io_batch* base,
                            struct vk_fd_table* fd_table_ptr,
                            struct vk_kern* kern_ptr,
                            size_t submitted)
{
    struct vk_io_uring_batch* batch = uring_from_base(base);
    if (!batch || !batch->driver) {
        return 0;
    }

    struct io_uring_cqe* cqe = NULL;
    size_t wait_target = submitted;
    if (wait_target == 0) {
        wait_target = batch->driver->pending_sqes;
    }

    if (wait_target > 0) {
        struct __kernel_timespec timeout = batch->driver->wait_timeout;
        int rc;
        if (timeout.tv_sec == 0 && timeout.tv_nsec == 0) {
            rc = io_uring_wait_cqe(&batch->driver->ring, &cqe);
        } else {
            rc = io_uring_wait_cqe_timeout(&batch->driver->ring, &cqe, &timeout);
        }
        if (rc == -EAGAIN || rc == -ETIME) {
            cqe = NULL;
        } else if (rc == -EINTR) {
            cqe = NULL;
        } else if (rc < 0) {
            errno = -rc;
            PERROR("io_uring_wait_cqe_timeout");
            return -1;
        }
        if (cqe) {
            goto process_single;
        }
    }

    while (io_uring_peek_cqe(&batch->driver->ring, &cqe) == 0) {
process_single:
        struct vk_fd_uring_meta* meta = cqe ? io_uring_cqe_get_data(cqe) : NULL;
        if (meta) {
            if (meta->kind == VK_FD_URING_META_POLL) {
                if (batch->driver->pending_poll > 0) {
                    batch->driver->pending_poll--;
                }
            } else if (meta->kind == VK_FD_URING_META_READ || meta->kind == VK_FD_URING_META_WRITE) {
                if (batch->driver->pending_rw > 0) {
                    batch->driver->pending_rw--;
                }
            }

            if (meta->fd) {
                struct vk_fd* fd_ptr = meta->fd;
                DBG("io_uring cqe fd=%d kind=%d res=%d\n",
                    fd_ptr->fd, meta->kind, (int)cqe->res);
                if (meta->kind == VK_FD_URING_META_POLL) {
                    vk_io_batch_uring_finalize_poll(fd_table_ptr, fd_ptr, (unsigned)cqe->res);
                } else {
                    struct vk_io_op* op_ptr = meta->op;
                    meta->op = NULL;
                    fd_ptr->uring.in_flight_rw = 0;
                    fd_ptr->uring.pending_rw = NULL;
                    if (op_ptr) {
                        vk_io_batch_uring_finalize_rw(fd_table_ptr, fd_ptr, op_ptr, (ssize_t)cqe->res);
                    } else {
                        vk_fd_table_clean_fd(fd_table_ptr, fd_ptr);
                    }
                }
            }
        }
        if (batch->driver->pending_sqes > 0) {
            batch->driver->pending_sqes--;
        }
        if (cqe) {
            io_uring_cqe_seen(&batch->driver->ring, cqe);
        }
        cqe = NULL;
    }

    (void)kern_ptr;
    return 0;
}

static int
vk_io_batch_uring_had_poll_ops_impl(const struct vk_io_batch* base)
{
    const struct vk_io_uring_batch* batch = uring_from_base_const(base);
    return batch ? (batch->staged_poll > 0) : 0;
}

static const struct vk_io_batch_ops vk_io_batch_uring_ops = {
    .reset = vk_io_batch_uring_reset_impl,
    .count = vk_io_batch_uring_count_impl,
    .stage_poll = vk_io_batch_uring_stage_poll_impl,
    .stage_rw = vk_io_batch_uring_stage_rw_impl,
    .submit = vk_io_batch_uring_submit_impl,
    .post_submit = vk_io_batch_uring_post_submit_impl,
    .reap = vk_io_batch_uring_reap_impl,
    .had_poll_ops = vk_io_batch_uring_had_poll_ops_impl,
};

void
vk_io_batch_uring_init(struct vk_io_uring_batch* batch,
                       struct vk_io_uring_driver* driver,
                       struct vk_fd_uring_meta** meta_list,
                       size_t max_entries)
{
    if (!batch) {
        return;
    }
    vk_io_batch_init(&batch->base, &vk_io_batch_uring_ops);
    batch->driver = driver;
    batch->meta_list = meta_list;
    batch->max_entries = max_entries;
    vk_io_batch_uring_reset_impl(&batch->base);
    if (driver) {
        driver->wait_timeout.tv_sec = 0;
        driver->wait_timeout.tv_nsec = 0;
    }
}

#endif /* VK_USE_IO_URING */
