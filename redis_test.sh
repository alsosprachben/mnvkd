#!/bin/sh
set -e

# Build targets we need
./debug.sh bmake vk_test_redis_service vk_test_redis_client_cli

# Start server (stderr to log), give it a moment to listen
./vk_test_redis_service 2>redis_server.log &
server_pid=$!
sleep 1

# Run client in foreground so we can wait for it; capture stdout and stderr
./vk_test_redis_client_cli < vk_test_redis_client.in.txt \
  > vk_test_redis_client.out.txt \
  2> redis_client.log || true

# Client sends SHUTDOWN, so server should exit on its own; wait for it
wait "$server_pid" 2>/dev/null || true

echo "Client and server finished. See vk_test_redis_client.out.txt, redis_client.log, redis_server.log"
