#include <string.h>

#include "vk_io_queue.h"
#include "vk_io_queue_s.h"
#include "vk_io_op_s.h"

size_t vk_io_queue_size(struct vk_io_queue* q)
{
    (void)q;
    return sizeof(struct vk_io_queue);
}

void vk_io_queue_init(struct vk_io_queue* q)
{
    memset(q, 0, sizeof(*q));
    TAILQ_INIT(&q->q);
}

void vk_io_queue_push(struct vk_io_queue* q, struct vk_io_op* op)
{
    TAILQ_INSERT_TAIL(&q->q, op, q_elem);
}

struct vk_io_op* vk_io_queue_pop(struct vk_io_queue* q)
{
    struct vk_io_op* op = TAILQ_FIRST(&q->q);
    if (op) {
        TAILQ_REMOVE(&q->q, op, q_elem);
    }
    return op;
}

struct vk_io_op* vk_io_queue_first(struct vk_io_queue* q)
{
    return TAILQ_FIRST(&q->q);
}

struct vk_io_op* vk_io_queue_next(struct vk_io_queue* q, struct vk_io_op* op)
{
    (void)q;
    return op ? TAILQ_NEXT(op, q_elem) : NULL;
}

void vk_io_queue_remove(struct vk_io_queue* q, struct vk_io_op* op)
{
    (void)q;
    if (op) {
        TAILQ_REMOVE(&((struct vk_io_queue*)q)->q, op, q_elem);
    }
}

int vk_io_queue_empty(struct vk_io_queue* q)
{
    return TAILQ_FIRST(&q->q) == NULL;
}

