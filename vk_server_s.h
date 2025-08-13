#ifndef VK_SERVER_S_H
#define VK_SERVER_S_H

#include "vk_pool_s.h"
#include "vk_thread.h"
#include <arpa/inet.h>
#include <sys/socket.h>

struct vk_server {
	struct vk_kern* kern_ptr;
	struct vk_pool* pool_ptr;

	int domain;
	int type;
	int protocol;

	struct sockaddr_storage address;
	socklen_t address_len;
	char address_str[INET_ADDRSTRLEN + 1];
	char port_str[6]; /* 5 digit port, plus \0 */

	int backlog;

	vk_func service_vk_func;
	size_t service_count;
	size_t service_page_count;
	void* service_msg;
        int privileged;
        int isolated;
        int reuseport;
        int ktls;
        const char* tls_cert;
        size_t tls_cert_len;
        const char* tls_key;
        size_t tls_key_len;
};

#endif
