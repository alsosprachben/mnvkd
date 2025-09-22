#ifndef VK_IO_QUEUE_S_H
#define VK_IO_QUEUE_S_H

#include "vk_queue.h"
#include "vk_io_queue.h"

struct vk_io_op;

/* forward declare tailq head */
TAILQ_HEAD(vk_io_op_head, vk_io_op);

struct vk_io_queue {
    struct vk_io_op_head phys_q;
    struct vk_io_op_head virt_q;
};

#endif /* VK_IO_QUEUE_S_H */
