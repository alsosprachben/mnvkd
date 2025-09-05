#include "vk_main_local.h"
#include "vk_redis.h"

int main(int argc, char* argv[])
{
        return vk_main_local_init(redis_request, NULL, 0, 44);
}
