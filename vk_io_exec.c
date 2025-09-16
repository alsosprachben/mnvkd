#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include "vk_io_exec.h"
#include "vk_io_op.h"
#include "vk_io_op_s.h"
#include "vk_vectoring.h"

int vk_io_exec_rw(struct vk_io_op* op)
{
    ssize_t rc;
    int iovcnt = op->iov[1].iov_len > 0 ? 2 : 1;

    switch (vk_io_op_get_kind(op)) {
        case VK_IO_READ:
            rc = readv(op->fd, op->iov, iovcnt);
            break;
        case VK_IO_WRITE:
            rc = writev(op->fd, op->iov, iovcnt);
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    op->res = rc;
    if (rc >= 0) {
        op->err = 0;
        /* If we transferred fewer bytes than requested, assume the socket
         * hit EAGAIN semantics for the remainder and require readiness
         * before continuing. The caller will advance rings by `rc` and
         * either rebuild or retry on the next pass. */
        if ((size_t)rc < op->len) {
            op->state = VK_IO_OP_NEEDS_POLL;
            op->err = EAGAIN;
        } else {
            op->state = VK_IO_OP_DONE;
        }
        return 0;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        op->err = errno;
        op->state = VK_IO_OP_NEEDS_POLL;
        return 0;
    }

    op->err = errno;
    op->state = VK_IO_OP_ERROR;
    return 0;
}

ssize_t vk_io_apply_tx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res)
{
    (void)op;
    return vk_vectoring_signed_sent(ring, res);
}

ssize_t vk_io_apply_rx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res)
{
    (void)op;
    return vk_vectoring_signed_received(ring, res);
}
