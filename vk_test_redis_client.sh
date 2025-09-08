#!/bin/sh
set -e
./vk_test_redis_service >/dev/null 2>&1 &
server_pid=$!
# Give server a moment to start
sleep 2
./vk_test_redis_client_encode < vk_test_redis_client.in.txt | nc localhost 6379 | ./vk_test_redis_client_decode > vk_test_redis_client.out.txt 2>/dev/null
# Give server a moment to finish logging
sleep 1
#./vk_test_redis_client_cli < vk_test_redis_client.in.txt > vk_test_redis_client.out.txt 2>/dev/null
wait $server_pid
