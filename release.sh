#!/bin/sh
if [ "$(uname)" = Linux ]
then
    CFLAGS='-O3 -DUSE_TLS=1 -D_GNU_SOURCE -flto -Wall' "${@}"
else
    CFLAGS='-O3 -DUSE_TLS=1 -flto -Wall' "${@}"
fi
ulimit -n 16384 # VK_FD_MAX
