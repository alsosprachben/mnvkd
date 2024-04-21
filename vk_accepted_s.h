#ifndef VK_ACCEPTED_S_H
#define VK_ACCEPTED_S_H

#include <arpa/inet.h>
#include <sys/socket.h>

struct vk_accepted {
	int fd;
	struct sockaddr_storage address;
	socklen_t address_len;
	char address_str[INET6_ADDRSTRLEN + 1];
	char port_str[6]; /* 5 digit port, plus \0 */
	struct vk_thread* vk_ptr;
	struct vk_proc* proc_ptr;
};

#endif