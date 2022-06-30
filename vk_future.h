#ifndef VK_FUTURE_H
#define VK_FUTURE_H

struct vk_future;
struct vk_thread;

void *vk_future_get(struct vk_future *ft_ptr);
void vk_future_bind(struct vk_future *ft_pr, struct vk_thread *vk_ptr);
void vk_future_resolve(struct vk_future *ft_ptr, void *msg);

#endif