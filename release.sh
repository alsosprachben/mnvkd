#!/bin/sh
if [ "$(uname)" = Linux ]
then
    CFLAGS='-Os -g3 -D_GNU_SOURCE -flto -Wall' "${@}"
else
    CFLAGS='-Os -flto -Wall' "${@}"
fi
