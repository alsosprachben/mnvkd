#ifndef VK_REDIS_H
#define VK_REDIS_H

#include "vk_thread.h"

#define REDIS_MAX_ARGS 8
#define REDIS_MAX_BULK 64

struct redis_query {
        int argc;
        int close;
        char argv[REDIS_MAX_ARGS][REDIS_MAX_BULK];
};

void redis_request(struct vk_thread* that);
void redis_response(struct vk_thread* that);
void redis_client(struct vk_thread* that);

#endif
