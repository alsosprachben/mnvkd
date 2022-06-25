#ifndef VK_ACCEPTED_S_H
#define VK_ACCEPTED_S_H

#include <sys/socket.h>
#include <arpa/inet.h>

struct vk_accepted {
		struct sockaddr_storage address;
		socklen_t address_len;
		char address_str[INET_ADDRSTRLEN + 1];
        char port_str[6]; /* 5 digit port, plus \0 */
		struct that *vk_ptr;
		struct vk_proc *proc_ptr;
};

#endif