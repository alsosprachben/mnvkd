#ifndef VK_SERVICE_S_H
#define VK_SERVICE_S_H

#include "vk_accepted_s.h"
#include "vk_server_s.h"

struct vk_service {
	struct vk_server server;
	struct vk_accepted accepted;
};

#endif