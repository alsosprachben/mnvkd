#ifndef VK_SERVER_S_H
#define VK_SERVER_S_H

struct vk_server {
	int domain;
	int type;
	int protocol;

	struct sockaddr *address;
	socklen_t address_len;

	int backlog;
};

#endif