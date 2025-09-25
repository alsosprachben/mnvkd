#ifndef VK_SYS_CAPS_S_H
#define VK_SYS_CAPS_S_H

struct vk_sys_caps {
    unsigned probed : 1;
    unsigned have_epoll : 1;
    unsigned have_kqueue : 1;
    unsigned have_aio : 1;
    unsigned have_aio_poll : 1;
};

#endif /* VK_SYS_CAPS_S_H */
