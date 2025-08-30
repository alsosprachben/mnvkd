#ifndef VK_CONNECTION_S_H
#define VK_CONNECTION_S_H

#include "vk_client_s.h"
#include "vk_accepted_s.h"

struct vk_connection {
    struct vk_client client;
    struct vk_accepted accepted;
};

#endif
