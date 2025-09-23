#ifndef VK_IO_EXEC_H
#define VK_IO_EXEC_H

#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

struct vk_io_op;
struct vk_vectoring;
enum VK_IO_OP_KIND;

typedef int (*vk_io_exec_fn)(struct vk_io_op* op);

int     vk_io_exec_register(enum VK_IO_OP_KIND kind, vk_io_exec_fn fn);

/* Execute a single I/O op. The dispatcher selects the appropriate syscall. */
int     vk_io_exec_op(struct vk_io_op* op);

/* Apply completion to rings (helpers for the caller). */
ssize_t vk_io_apply_tx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res);
ssize_t vk_io_apply_rx(struct vk_vectoring* ring, const struct vk_io_op* op, ssize_t res);

#endif /* VK_IO_EXEC_H */
