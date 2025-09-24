#include "vk_main.h"
#include "vk_redis.h"

int main() {
    /* Reads human commands on stdin, writes RESP to stdout. */
    return vk_main_init(redis_client_request, NULL, 0, 44, 1, 0);
}
