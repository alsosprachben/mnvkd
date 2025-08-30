#ifndef VK_CLIENT_S_H
#define VK_CLIENT_S_H

#include "vk_thread.h"

struct vk_client {
    const char* address;
    const char* port;
    vk_func vk_func;
};

#endif
