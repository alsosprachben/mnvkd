#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "vk_debug.h"
#include "vk_io_exec.h"
#include "vk_io_op.h"
#include "vk_io_op_s.h"
#include "vk_vectoring.h"
#include "vk_accepted.h"

typedef int (*vk_io_executor_fn)(struct vk_io_op*);

static int vk_io_exec_read(struct vk_io_op* op_ptr);
static int vk_io_exec_write(struct vk_io_op* op_ptr);
static int vk_io_exec_accept(struct vk_io_op* op_ptr);
static int vk_io_exec_close(struct vk_io_op* op_ptr);
static int vk_io_exec_shutdown(struct vk_io_op* op_ptr);
static int vk_io_exec_unimplemented(struct vk_io_op* op_ptr);

static vk_io_executor_fn vk_io_exec_table[VK_IO_OP_KIND_COUNT] = {
    [VK_IO_READ]     = vk_io_exec_read,
    [VK_IO_WRITE]    = vk_io_exec_write,
    [VK_IO_ACCEPT]   = vk_io_exec_accept,
    [VK_IO_CLOSE]    = vk_io_exec_close,
    [VK_IO_SHUTDOWN] = vk_io_exec_shutdown,
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

void
vk_io_exec_finalize_rw(struct vk_io_op* op_ptr, ssize_t rc)
{
    op_ptr->res = rc;
    if (rc >= 0) {
        op_ptr->err = 0;
        if (rc == 0 && (vk_io_op_get_flags(op_ptr) & VK_IO_F_DIR_RX)) {
            op_ptr->state = VK_IO_OP_DONE;
            DBG("io_exec: fd=%d kind=%d EOF rc=0 len=%zu\n",
                op_ptr->fd, op_ptr->kind, op_ptr->len);
        } else if ((size_t)rc < op_ptr->len) {
            op_ptr->state = VK_IO_OP_NEEDS_POLL;
            op_ptr->err = EAGAIN;
            DBG("io_exec: fd=%d kind=%d partial transfer rc=%zd len=%zu -> NEEDS_POLL\n",
                op_ptr->fd, op_ptr->kind, rc, op_ptr->len);
        } else {
            op_ptr->state = VK_IO_OP_DONE;
            DBG("io_exec: fd=%d kind=%d completed rc=%zd len=%zu\n",
                op_ptr->fd, op_ptr->kind, rc, op_ptr->len);
        }
        return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        op_ptr->res = 0;
        op_ptr->err = errno;
        op_ptr->state = VK_IO_OP_NEEDS_POLL;
        DBG("io_exec: fd=%d kind=%d hit EAGAIN\n", op_ptr->fd, op_ptr->kind);
        return;
    }

    op_ptr->err = errno;
    op_ptr->state = VK_IO_OP_ERROR;
    DBG("io_exec: fd=%d kind=%d error errno=%d\n", op_ptr->fd, op_ptr->kind, errno);
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
vk_io_exec_accept(struct vk_io_op* op_ptr)
{
    struct vk_accepted* accepted_ptr;
    size_t need = vk_accepted_alloc_size();

    if (op_ptr->iov[0].iov_len < need) {
        op_ptr->res = -1;
        op_ptr->err = EINVAL;
        op_ptr->state = VK_IO_OP_ERROR;
        errno = EINVAL;
        return -1;
    }

    accepted_ptr = (struct vk_accepted*)op_ptr->iov[0].iov_base;
    ssize_t rc = vk_accepted_accept(accepted_ptr, op_ptr->fd);
    DBG("io_exec: accept fd=%d rc=%zd\n", op_ptr->fd, rc);

    if (rc >= 0) {
        op_ptr->res = (ssize_t)need;
        op_ptr->err = 0;
        op_ptr->state = VK_IO_OP_DONE;
        return 0;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        op_ptr->res = 0;
        op_ptr->err = errno;
        op_ptr->state = VK_IO_OP_NEEDS_POLL;
        return 0;
    }

    op_ptr->res = -1;
    op_ptr->err = errno;
    op_ptr->state = VK_IO_OP_ERROR;
    return -1;
}

static int
vk_io_exec_close(struct vk_io_op* op_ptr)
{
    int fd = op_ptr->fd;
    int rc;

    if (fd < 0) {
        op_ptr->res = 0;
        op_ptr->err = 0;
        op_ptr->state = VK_IO_OP_DONE;
        return 0;
    }

retry:
    rc = close(fd);
    DBG("io_exec: close fd=%d rc=%d errno=%d\n", fd, rc, rc == -1 ? errno : 0);
    if (rc == 0) {
        op_ptr->res = 0;
        op_ptr->err = 0;
        op_ptr->state = VK_IO_OP_DONE;
        op_ptr->fd = -1;
        return 0;
    }

    if (rc == -1 && errno == EINTR) {
        goto retry;
    }

    op_ptr->res = -1;
    op_ptr->err = errno;
    op_ptr->state = VK_IO_OP_ERROR;
    return -1;
}

static int
vk_io_exec_shutdown(struct vk_io_op* op_ptr)
{
    int fd = op_ptr->fd;
    int how = (int)(intptr_t)op_ptr->tag2;
    int rc;

    if (fd < 0) {
        op_ptr->res = 0;
        op_ptr->err = 0;
        op_ptr->state = VK_IO_OP_DONE;
        return 0;
    }

retry:
    rc = shutdown(fd, how);
    DBG("io_exec: shutdown fd=%d how=%d rc=%d errno=%d\n", fd, how, rc, rc == -1 ? errno : 0);
    if (rc == 0) {
        op_ptr->res = 0;
        op_ptr->err = 0;
        op_ptr->state = VK_IO_OP_DONE;
        return 0;
    }

    if (rc == -1 && errno == EINTR) {
        goto retry;
    }

    op_ptr->res = -1;
    op_ptr->err = errno;
    op_ptr->state = VK_IO_OP_ERROR;
    return -1;
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
