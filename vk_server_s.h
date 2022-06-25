#ifndef VK_SERVER_S_H
#define VK_SERVER_S_H

#include "vk_state.h"
#include <sys/socket.h>
#include <arpa/inet.h>

struct vk_server {
	int domain;
	int type;
	int protocol;

	struct sockaddr_storage address;
	socklen_t address_len;
	char address_str[INET_ADDRSTRLEN];

	int backlog;

    vk_func service_vk_func;
    void *service_msg;
};

#endif