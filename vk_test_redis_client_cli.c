#include "vk_client.h"
#include "vk_redis.h"
#include <stdlib.h>

int main(int argc, char* argv[])
{
	struct vk_client* client;
	int rc;
	
	client = calloc(1, vk_client_alloc_size());
	if (client == NULL) {
		return 1;
	}
	vk_client_set_address(client, "127.0.0.1");
	vk_client_set_port(client, "6379");
	vk_client_set_vk_func(client, redis_client);
	
	rc = vk_client_init(client, 44, 1, 0);
	free(client);
	return rc;
}
