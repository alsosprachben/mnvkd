#!/bin/sh
if [ "$(uname)" = Linux ]
then
    CFLAGS='-Os -D_GNU_SOURCE -flto -Wall' "${@}"
else
    CFLAGS='-Os -flto -Wall' "${@}"
fi