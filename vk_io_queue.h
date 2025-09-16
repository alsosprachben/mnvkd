#ifndef VK_IO_QUEUE_H
#define VK_IO_QUEUE_H

#include <stddef.h>

struct vk_io_op;
struct vk_io_queue;

size_t vk_io_queue_size(struct vk_io_queue* q);
void   vk_io_queue_init(struct vk_io_queue* q);

/* enqueue op at tail */
void   vk_io_queue_push(struct vk_io_queue* q, struct vk_io_op* op);

/* dequeue op from head; returns NULL if empty */
struct vk_io_op* vk_io_queue_pop(struct vk_io_queue* q);

/* peek head */
struct vk_io_op* vk_io_queue_first(struct vk_io_queue* q);

/* next element after given op (or NULL) */
struct vk_io_op* vk_io_queue_next(struct vk_io_queue* q, struct vk_io_op* op);

/* remove specific op from queue if present */
void   vk_io_queue_remove(struct vk_io_queue* q, struct vk_io_op* op);

/* empty? */
int    vk_io_queue_empty(struct vk_io_queue* q);

#endif /* VK_IO_QUEUE_H */

