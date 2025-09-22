#ifndef VK_IO_QUEUE_H
#define VK_IO_QUEUE_H

#include <stddef.h>

struct vk_io_op;
struct vk_io_queue;

size_t vk_io_queue_size(struct vk_io_queue* q);
void   vk_io_queue_init(struct vk_io_queue* q);

/* enqueue op at tail of physical or virtual queue */
void   vk_io_queue_push(struct vk_io_queue* q, struct vk_io_op* op);

/* dequeue from specific queues; return NULL if empty */
struct vk_io_op* vk_io_queue_pop_phys(struct vk_io_queue* q);
struct vk_io_op* vk_io_queue_pop_virt(struct vk_io_queue* q);

/* peek head of specific queues */
struct vk_io_op* vk_io_queue_first_phys(struct vk_io_queue* q);
struct vk_io_op* vk_io_queue_first_virt(struct vk_io_queue* q);

/* remove specific op from its queue if present */
void   vk_io_queue_remove(struct vk_io_queue* q, struct vk_io_op* op);

/* empty checks */
int    vk_io_queue_empty(struct vk_io_queue* q);
int    vk_io_queue_empty_phys(struct vk_io_queue* q);
int    vk_io_queue_empty_virt(struct vk_io_queue* q);

#endif /* VK_IO_QUEUE_H */
