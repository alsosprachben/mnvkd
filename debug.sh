#!/bin/sh
if [ "$(uname)" = Linux ]
then
    CFLAGS='-O0 -g3 -DUSE_TLS=1 -D_GNU_SOURCE -DDEBUG=1 -Wall' "${@}"
else
    CFLAGS='-O0 -g3 -DUSE_TLS=1 -DDEBUG=1 -Wall' "${@}"
fi
ulimit -n 16384 # VK_FD_MAX
