#!/bin/sh
set -e
./vk_test_responder_join_service >/dev/null 2>responder-join-service.err.log &
server_pid=$!
cleanup() {
    kill ${server_pid} 2>/dev/null || true
}
trap cleanup EXIT
sleep 1
timeout 10 python - <<'PY'
import socket

with open('vk_test_responder_join.in.txt', 'rb') as inp:
    lines = inp.readlines()

s = socket.create_connection(('127.0.0.1', 9089), timeout=2)
for line in lines:
    s.sendall(line)
s.shutdown(socket.SHUT_WR)

data = bytearray()
while True:
    chunk = s.recv(4096)
    if not chunk:
        break
    data.extend(chunk)
s.close()

with open('vk_test_responder_join_service.out.txt', 'wb') as out:
    out.write(data)
PY
kill ${server_pid} 2>/dev/null || true
wait ${server_pid} 2>/dev/null || true
