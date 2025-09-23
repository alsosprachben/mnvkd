#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include "vk_debug.h"
#include "vk_io_exec.h"
#include "vk_io_op.h"
#include "vk_io_op_s.h"
#include "vk_vectoring.h"

typedef int (*vk_io_executor_fn)(struct vk_io_op*);

static int vk_io_exec_read(struct vk_io_op* op_ptr);
static int vk_io_exec_write(struct vk_io_op* op_ptr);
static int vk_io_exec_unimplemented(struct vk_io_op* op_ptr);

static vk_io_executor_fn vk_io_exec_table[VK_IO_OP_KIND_COUNT] = {
    [VK_IO_READ]     = vk_io_exec_read,
    [VK_IO_WRITE]    = vk_io_exec_write,
    [VK_IO_ACCEPT]   = vk_io_exec_unimplemented,
    [VK_IO_CLOSE]    = vk_io_exec_unimplemented,
    [VK_IO_SHUTDOWN] = vk_io_exec_unimplemented,
};

int
vk_io_exec_register(enum VK_IO_OP_KIND kind, vk_io_exec_fn fn)
{
    if (kind < 0 || kind >= VK_IO_OP_KIND_COUNT) {
        errno = EINVAL;
        return -1;
    }
    vk_io_exec_table[kind] = fn ? fn : vk_io_exec_unimplemented;
    return 0;
}

static void
vk_io_exec_finalize_rw(struct vk_io_op* op_ptr, ssize_t rc)
{
    op_ptr->res = rc;
    if (rc >= 0) {
        op_ptr->err = 0;
        if ((size_t)rc < op_ptr->len) {
            op_ptr->state = VK_IO_OP_NEEDS_POLL;
            op_ptr->err = EAGAIN;
        } else {
            op_ptr->state = VK_IO_OP_DONE;
        }
        return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        op_ptr->err = errno;
        op_ptr->state = VK_IO_OP_NEEDS_POLL;
        return;
    }

    op_ptr->err = errno;
    op_ptr->state = VK_IO_OP_ERROR;
}

static int
vk_io_exec_read(struct vk_io_op* op_ptr)
{
    int iovcnt = op_ptr->iov[1].iov_len > 0 ? 2 : 1;
    ssize_t rc = readv(op_ptr->fd, op_ptr->iov, iovcnt);
    DBG("io_exec: read fd=%d rc=%zd len=%zu\n", op_ptr->fd, rc, op_ptr->len);
    vk_io_exec_finalize_rw(op_ptr, rc);
    return 0;
}

static int
vk_io_exec_write(struct vk_io_op* op_ptr)
{
    int iovcnt = op_ptr->iov[1].iov_len > 0 ? 2 : 1;
    ssize_t rc = writev(op_ptr->fd, op_ptr->iov, iovcnt);
    DBG("io_exec: write fd=%d rc=%zd len=%zu\n", op_ptr->fd, rc, op_ptr->len);
    vk_io_exec_finalize_rw(op_ptr, rc);
    return 0;
}

static int
vk_io_exec_unimplemented(struct vk_io_op* op_ptr)
{
    op_ptr->res = -1;
    op_ptr->err = ENOSYS;
    op_ptr->state = VK_IO_OP_ERROR;
    errno = ENOSYS;
    return -1;
}

int
vk_io_exec_op(struct vk_io_op* op_ptr)
{
    if (!op_ptr) {
        errno = EINVAL;
        return -1;
    }

    enum VK_IO_OP_KIND kind = vk_io_op_get_kind(op_ptr);
    if (kind < 0 || kind >= VK_IO_OP_KIND_COUNT) {
        op_ptr->res = -1;
        op_ptr->err = EINVAL;
        op_ptr->state = VK_IO_OP_ERROR;
        errno = EINVAL;
        return -1;
    }

    vk_io_executor_fn fn = vk_io_exec_table[kind];
    if (!fn) {
        fn = vk_io_exec_unimplemented;
    }
    return fn(op_ptr);
}

ssize_t
vk_io_apply_tx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res)
{
    (void)op;
    return vk_vectoring_signed_sent(ring, res);
}

ssize_t
vk_io_apply_rx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res)
{
    (void)op;
    return vk_vectoring_signed_received(ring, res);
}

