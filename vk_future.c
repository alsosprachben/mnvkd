#include "vk_future.h"
#include "vk_future_s.h"

#include <stddef.h>

void *vk_future_get(struct vk_future *ft_ptr) {
    return ft_ptr->msg;
}

void vk_future_bind(struct vk_future *ft_ptr, struct vk_thread *vk_ptr) {
    ft_ptr->vk = vk_ptr;
}

void vk_future_resolve(struct vk_future *ft_ptr, void *msg) {
    ft_ptr->msg = msg;
}