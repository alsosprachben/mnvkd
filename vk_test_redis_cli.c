#include "vk_main.h"
#include "vk_redis.h"

int main(int argc, char* argv[])
{
        /* Integrate with kernel + poller so stdin/stdout participate in readiness */
        return vk_main_init(redis_request, NULL, 0, 44, 1, 0);
}
