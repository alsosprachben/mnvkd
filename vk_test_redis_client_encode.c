#include "vk_main_local.h"
#include "vk_redis.h"

int main() {
    /* Reads human commands on stdin, writes RESP to stdout. */
    return vk_main_local_init(redis_client_request, NULL, 0, 44);
}

