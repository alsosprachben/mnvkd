#!/bin/sh
./debug.sh bmake clean
./debug.sh bmake vk_test_redis_service vk_test_redis_client_cli
./vk_test_redis_service 2>redis_server.log &
sleep 2
./vk_test_redis_client_cli < vk_test_redis_client.in.txt 2> redis_client.log &
sleep 5
kill -9 %1
kill -9 %1
jobs
