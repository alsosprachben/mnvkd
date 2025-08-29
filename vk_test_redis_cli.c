#include "vk_main.h"
#include "vk_redis.h"

int main(int argc, char* argv[])
{
        return vk_main_init(redis_request, NULL, 0, 25, 0, 1);
}
