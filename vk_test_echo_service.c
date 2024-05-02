/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_service.h"
#include "vk_echo.h"

#include <netinet/in.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	int rc;
	struct vk_server* server_ptr;
	struct vk_pool* pool_ptr;
	struct sockaddr_in address;

	server_ptr = calloc(1, vk_server_alloc_size());
	pool_ptr = calloc(1, vk_pool_alloc_size());

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	vk_server_set_pool(server_ptr, pool_ptr);
	vk_server_set_socket(server_ptr, PF_INET, SOCK_STREAM, 0);
	vk_server_set_address(server_ptr, (struct sockaddr*)&address, sizeof(address));
	vk_server_set_backlog(server_ptr, 128);
	vk_server_set_vk_func(server_ptr, vk_echo);
	vk_server_set_count(server_ptr, 0);
	vk_server_set_privileged(server_ptr, 0);
	vk_server_set_isolated(server_ptr, 1);
	vk_server_set_page_count(server_ptr, 25);
	vk_server_set_msg(server_ptr, NULL);
	rc = vk_server_init(server_ptr);
	if (rc == -1) {
		return 1;
	}

	return 0;
}
