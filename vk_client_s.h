#ifndef VK_CLIENT_S_H
#define VK_CLIENT_S_H

#include "vk_thread.h"

struct vk_client {
    const char* address;
    const char* port;
    /* request: stdin -> socket */
    vk_func vk_req_func;
    /* response: socket -> stdout */
    vk_func vk_resp_func;
};

#endif
