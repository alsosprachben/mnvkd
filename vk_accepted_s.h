#ifndef VK_ACCEPTED_S_H
#define VK_ACCEPTED_S_H

#include <sys/socket.h>
#include <arpa/inet.h>

struct vk_accepted {
		int accepted_fd;
		struct sockaddr client_address;
		socklen_t client_address_len;
		char client_address_str[INET_ADDRSTRLEN];
		struct that *accepted_vk_ptr;
		struct vk_proc *accepted_proc_ptr;
};

#endif