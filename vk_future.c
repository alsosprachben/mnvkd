#include "vk_future.h"
#include "vk_future_s.h"

void *vk_future_get(struct vk_future *ft_ptr) {
    return ft_ptr->msg;
}

void vk_future_bind(struct vk_future *ft_ptr, struct that *vk_ptr) {
    ft_ptr->vk = vk_ptr;
}

void vk_future_resolve(struct vk_future *ft_ptr, void *msg) {
    ft_ptr->msg = msg;
}
