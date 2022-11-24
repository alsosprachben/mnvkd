#!/bin/sh
if [ "$(uname)" = Linux ]
then
    CFLAGS='-O0 -g3 -D_GNU_SOURCE -DDEBUG=1 -Wall' "${@}"
else
    CFLAGS='-O0 -g3 -DDEBUG=1 -Wall' "${@}"
fi