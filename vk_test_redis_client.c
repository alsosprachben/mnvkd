#include "vk_client.h"
#include "vk_redis.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    const char* addr = "127.0.0.1";
    const char* port = "6379";
    const char* infile = "vk_test_redis_client.in.txt";
    int fd = -1;
    struct vk_client* client;
    int rc;

    if (argc > 1 && argv[1] && *argv[1]) addr = argv[1];
    if (argc > 2 && argv[2] && *argv[2]) port = argv[2];
    if (argc > 3 && argv[3] && *argv[3]) infile = argv[3];

    fd = open(infile, O_RDONLY);
    if (fd == -1) {
        perror("open input");
        return 1;
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        perror("dup2");
        close(fd);
        return 1;
    }
    close(fd);

    client = calloc(1, vk_client_alloc_size());
    if (client == NULL) {
        return 1;
    }
    vk_client_set_address(client, addr);
    vk_client_set_port(client, port);
    vk_client_set_vk_req_func(client, redis_client_request);
    vk_client_set_vk_resp_func(client, redis_client_response);

    rc = vk_client_init(client, 60, 1, 0);
    free(client);
    return rc;
}

