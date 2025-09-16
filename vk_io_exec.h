#ifndef VK_IO_EXEC_H
#define VK_IO_EXEC_H

#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

struct vk_io_op;
struct vk_vectoring;

/* Execute a single READ/WRITE op using nonblocking readv/writev. */
int     vk_io_exec_rw(struct vk_io_op* op);

/* Apply completion to rings (helpers for the caller). */
ssize_t vk_io_apply_tx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res);
ssize_t vk_io_apply_rx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res);

#endif /* VK_IO_EXEC_H */

