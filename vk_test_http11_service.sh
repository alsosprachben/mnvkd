#!/bin/sh
set -e
./vk_test_http11_service >/dev/null 2>&1 &
server_pid=$!
# Give server a moment to start
sleep 1
nc localhost 8081 < vk_test_http11.in3.txt > vk_test_http11_service.out3.txt 2>/dev/null
sleep 1
wait $server_pid
