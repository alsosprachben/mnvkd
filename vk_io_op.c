#include <errno.h>
#include <string.h>

#include "vk_io_op.h"
#include "vk_io_op_s.h"

#include "vk_vectoring.h"

size_t vk_io_op_size(struct vk_io_op* op_ptr) { (void)op_ptr; return sizeof(struct vk_io_op); }

void vk_io_op_init(struct vk_io_op* op_ptr)
{
    memset(op_ptr, 0, sizeof(*op_ptr));
}

void vk_io_op_set_fd(struct vk_io_op* op_ptr, int fd) { op_ptr->fd = fd; }
int  vk_io_op_get_fd(struct vk_io_op* op_ptr) { return op_ptr->fd; }

void vk_io_op_set_proc(struct vk_io_op* op_ptr, struct vk_proc* proc_ptr) { op_ptr->proc_ptr = proc_ptr; }
struct vk_proc* vk_io_op_get_proc(struct vk_io_op* op_ptr) { return op_ptr->proc_ptr; }

void vk_io_op_set_thread(struct vk_io_op* op_ptr, struct vk_thread* thread_ptr) { op_ptr->thread_ptr = thread_ptr; }
struct vk_thread* vk_io_op_get_thread(struct vk_io_op* op_ptr) { return op_ptr->thread_ptr; }

void vk_io_op_set_kind(struct vk_io_op* op_ptr, enum VK_IO_OP_KIND kind) { op_ptr->kind = kind; }
enum VK_IO_OP_KIND vk_io_op_get_kind(struct vk_io_op* op_ptr) { return op_ptr->kind; }

void vk_io_op_set_flags(struct vk_io_op* op_ptr, unsigned flags) { op_ptr->flags = flags; }
unsigned vk_io_op_get_flags(struct vk_io_op* op_ptr) { return op_ptr->flags; }

void vk_io_op_set_state(struct vk_io_op* op_ptr, enum VK_IO_OP_STATE st) { op_ptr->state = st; }
enum VK_IO_OP_STATE vk_io_op_get_state(struct vk_io_op* op_ptr) { return op_ptr->state; }

void vk_io_op_set_len(struct vk_io_op* op_ptr, size_t len) { op_ptr->len = len; }
size_t vk_io_op_get_len(struct vk_io_op* op_ptr) { return op_ptr->len; }

struct iovec* vk_io_op_get_iov(struct vk_io_op* op_ptr) { return op_ptr->iov; }

void vk_io_op_set_iov2(struct vk_io_op* op_ptr, const struct iovec iov[2], size_t total_len)
{
    op_ptr->iov[0] = iov[0];
    op_ptr->iov[1] = iov[1];
    op_ptr->len = total_len;
}

static size_t min_size(size_t a, size_t b) { return a < b ? a : b; }

/* Build an iovec view from the TX side (filled bytes available to send). */
int vk_io_op_from_tx_ring(struct vk_io_op* op_ptr, struct vk_vectoring* ring, size_t max_len)
{
    size_t b1 = (size_t)vk_vectoring_tx_buf1_len(ring);
    size_t b2 = (size_t)vk_vectoring_tx_buf2_len(ring);
    char*  p1 = vk_vectoring_tx_buf1(ring);
    char*  p2 = vk_vectoring_tx_buf2(ring);

    if (b1 + b2 == 0) {
        errno = EAGAIN;
        return -1;
    }

    size_t need = max_len > 0 ? max_len : (b1 + b2);
    size_t l1 = min_size(b1, need);
    size_t remaining = need - l1;
    size_t l2 = remaining > 0 ? min_size(b2, remaining) : 0;

    op_ptr->iov[0].iov_base = p1;
    op_ptr->iov[0].iov_len  = l1;
    op_ptr->iov[1].iov_base = p2;
    op_ptr->iov[1].iov_len  = l2;
    op_ptr->len             = l1 + l2;
    return 0;
}

/* Build an iovec view from the RX side (empty space available to receive). */
int vk_io_op_from_rx_ring(struct vk_io_op* op_ptr, struct vk_vectoring* ring, size_t max_len)
{
    size_t b1 = (size_t)vk_vectoring_rx_buf1_len(ring);
    size_t b2 = (size_t)vk_vectoring_rx_buf2_len(ring);
    char*  p1 = vk_vectoring_rx_buf1(ring);
    char*  p2 = vk_vectoring_rx_buf2(ring);

    if (b1 + b2 == 0) {
        errno = EAGAIN;
        return -1;
    }

    size_t need = max_len > 0 ? max_len : (b1 + b2);
    size_t l1 = min_size(b1, need);
    size_t remaining = need - l1;
    size_t l2 = remaining > 0 ? min_size(b2, remaining) : 0;

    op_ptr->iov[0].iov_base = p1;
    op_ptr->iov[0].iov_len  = l1;
    op_ptr->iov[1].iov_base = p2;
    op_ptr->iov[1].iov_len  = l2;
    op_ptr->len             = l1 + l2;
    return 0;
}

