#ifndef VK_IO_OP_S_H
#define VK_IO_OP_S_H

#include <sys/uio.h>
#include <stddef.h>
#include "vk_queue.h"

struct vk_proc;
struct vk_thread;

enum VK_IO_OP_KIND;
enum VK_IO_OP_STATE;

struct vk_io_op {
    int                      fd;
    struct vk_proc*          proc_ptr;
    struct vk_thread*        thread_ptr;
    enum VK_IO_OP_KIND       kind;
    struct iovec             iov[2];
    size_t                   len;
    unsigned                 flags;
    enum VK_IO_OP_STATE      state;
    ssize_t                  res;      /* last result, if any */
    int                      err;      /* sticky error code if state==ERROR */
    void*                    tag1;     /* backend tag (e.g., iocb*, sqe*) */
    void*                    tag2;     /* backend tag */
    TAILQ_ENTRY(vk_io_op)    q_elem;   /* link in vk_io_queue */
};

#endif /* VK_IO_OP_S_H */
