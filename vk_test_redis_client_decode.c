#include "vk_main.h"
#include "vk_redis.h"

int main() {
    /* Reads RESP on stdin, writes human-formatted replies to stdout. */
    return vk_main_init(redis_client_response, NULL, 0, 44, 1, 0);
}
