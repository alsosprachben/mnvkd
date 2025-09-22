#ifndef VK_IO_OP_H
#define VK_IO_OP_H

#include <sys/uio.h>
#include <stddef.h>

/* forward decls to avoid including heavy headers here */
struct vk_proc;
struct vk_thread;
struct vk_vectoring;

/*
 * I/O operation kind
 */
enum VK_IO_OP_KIND {
    VK_IO_READ = 0,
    VK_IO_WRITE,
    VK_IO_ACCEPT,
    VK_IO_CLOSE,
    VK_IO_SHUTDOWN,
};

/*
 * I/O operation lifecycle
 */
enum VK_IO_OP_STATE {
    VK_IO_OP_PENDING = 0,
    VK_IO_OP_STAGED,
    VK_IO_OP_SUBMITTED,
    VK_IO_OP_DONE,
    VK_IO_OP_EAGAIN,
    VK_IO_OP_NEEDS_POLL,
    VK_IO_OP_ERROR,
    VK_IO_OP_CANCELED,
};

/* flags */
#define VK_IO_F_SOCKET         (1u << 0)
#define VK_IO_F_STREAM         (1u << 1)
#define VK_IO_F_ALLOW_PARTIAL  (1u << 2)
#define VK_IO_F_DIR_RX         (1u << 3)
#define VK_IO_F_DIR_TX         (1u << 4)

/* opaque type */
struct vk_io_op;

/* basic lifecycle */
size_t vk_io_op_size(struct vk_io_op* op_ptr);
void   vk_io_op_init(struct vk_io_op* op_ptr);

/* configuration */
void   vk_io_op_set_fd(struct vk_io_op* op_ptr, int fd);
int    vk_io_op_get_fd(struct vk_io_op* op_ptr);

void   vk_io_op_set_proc(struct vk_io_op* op_ptr, struct vk_proc* proc_ptr);
struct vk_proc* vk_io_op_get_proc(struct vk_io_op* op_ptr);

void   vk_io_op_set_thread(struct vk_io_op* op_ptr, struct vk_thread* thread_ptr);
struct vk_thread* vk_io_op_get_thread(struct vk_io_op* op_ptr);

void   vk_io_op_set_kind(struct vk_io_op* op_ptr, enum VK_IO_OP_KIND kind);
enum VK_IO_OP_KIND vk_io_op_get_kind(struct vk_io_op* op_ptr);

void   vk_io_op_set_flags(struct vk_io_op* op_ptr, unsigned flags);
unsigned vk_io_op_get_flags(struct vk_io_op* op_ptr);

void   vk_io_op_set_state(struct vk_io_op* op_ptr, enum VK_IO_OP_STATE st);
enum VK_IO_OP_STATE vk_io_op_get_state(struct vk_io_op* op_ptr);

void   vk_io_op_set_len(struct vk_io_op* op_ptr, size_t len);
size_t vk_io_op_get_len(struct vk_io_op* op_ptr);

/* access iovec view (two segments max) */
struct iovec* vk_io_op_get_iov(struct vk_io_op* op_ptr);

/* set iov explicitly */
void   vk_io_op_set_iov2(struct vk_io_op* op_ptr, const struct iovec iov[2], size_t total_len);

/*
 * Convenience builders from vector rings
 *  - WRITE: build from TX view (filled bytes to send)
 *  - READ:  build from RX view (empty bytes to receive)
 * Returns 0 on success, -1 if nothing to do (errno set to EAGAIN)
 */
int vk_io_op_from_tx_ring(struct vk_io_op* op_ptr, struct vk_vectoring* ring, size_t max_len);
int vk_io_op_from_rx_ring(struct vk_io_op* op_ptr, struct vk_vectoring* ring, size_t max_len);

#endif /* VK_IO_OP_H */
