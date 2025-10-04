#!/bin/sh
set -e
./vk_test_http11_service >/dev/null 2>http11-err.log &
server_pid=$!
# Give server a moment to start
sleep 2
printf "in3 test\n" >>/dev/stderr
nc localhost 8081 < vk_test_http11.in3.txt        > vk_test_http11_service.out3.txt 2>/dev/null
printf "in3post test\n" >>/dev/stderr
nc localhost 8081 < vk_test_http11.in3post.txt    > vk_test_http11_service.out3post.txt 2>/dev/null
printf "in3chunked test\n" >>/dev/stderr
nc localhost 8081 < vk_test_http11.in3chunked.txt > vk_test_http11_service.out3chunked.txt 2>/dev/null

sleep 1
kill -9 $server_pid
#wait $server_pid
