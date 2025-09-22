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
    TAILQ_INIT(&q->phys_q);
    TAILQ_INIT(&q->virt_q);
}

static inline struct vk_io_op_head*
vk_io_queue_head_for(struct vk_io_queue* q, struct vk_io_op* op)
{
    return (op && op->fd < 0) ? &q->virt_q : &q->phys_q;
}

void vk_io_queue_push(struct vk_io_queue* q, struct vk_io_op* op)
{
    TAILQ_INSERT_TAIL(vk_io_queue_head_for(q, op), op, q_elem);
}

struct vk_io_op* vk_io_queue_pop_phys(struct vk_io_queue* q)
{
    struct vk_io_op* op = TAILQ_FIRST(&q->phys_q);
    if (op) {
        TAILQ_REMOVE(&q->phys_q, op, q_elem);
    }
    return op;
}

struct vk_io_op* vk_io_queue_pop_virt(struct vk_io_queue* q)
{
    struct vk_io_op* op = TAILQ_FIRST(&q->virt_q);
    if (op) {
        TAILQ_REMOVE(&q->virt_q, op, q_elem);
    }
    return op;
}

struct vk_io_op* vk_io_queue_first_phys(struct vk_io_queue* q)
{
    return TAILQ_FIRST(&q->phys_q);
}

struct vk_io_op* vk_io_queue_first_virt(struct vk_io_queue* q)
{
    return TAILQ_FIRST(&q->virt_q);
}

void vk_io_queue_remove(struct vk_io_queue* q, struct vk_io_op* op)
{
    if (op == NULL || op->q_elem.tqe_prev == NULL) {
        return;
    }
    TAILQ_REMOVE(vk_io_queue_head_for(q, op), op, q_elem);
}

int vk_io_queue_empty_phys(struct vk_io_queue* q)
{
    return TAILQ_FIRST(&q->phys_q) == NULL;
}

int vk_io_queue_empty_virt(struct vk_io_queue* q)
{
    return TAILQ_FIRST(&q->virt_q) == NULL;
}

int vk_io_queue_empty(struct vk_io_queue* q)
{
    return vk_io_queue_empty_phys(q) && vk_io_queue_empty_virt(q);
}
