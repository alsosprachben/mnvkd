#!/bin/sh
if [ "$(uname)" = Linux ]
then
    CFLAGS='-O3 -D_GNU_SOURCE -flto -Wall' "${@}"
else
    CFLAGS='-O3 -flto -Wall' "${@}"
fi
ulimit -n 16384 # VK_FD_MAX