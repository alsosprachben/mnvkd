#ifndef VK_SYS_CAPS_H
#define VK_SYS_CAPS_H

struct vk_sys_caps;

void vk_sys_caps_init(struct vk_sys_caps* caps);
int  vk_sys_caps_detect(struct vk_sys_caps* caps);

int vk_sys_caps_is_probed(const struct vk_sys_caps* caps);
int vk_sys_caps_have_epoll(const struct vk_sys_caps* caps);
int vk_sys_caps_have_kqueue(const struct vk_sys_caps* caps);
int vk_sys_caps_have_aio(const struct vk_sys_caps* caps);
int vk_sys_caps_have_aio_poll(const struct vk_sys_caps* caps);
int vk_sys_caps_have_io_uring(const struct vk_sys_caps* caps);
int vk_sys_caps_have_io_uring_fast_poll(const struct vk_sys_caps* caps);

#endif /* VK_SYS_CAPS_H */
