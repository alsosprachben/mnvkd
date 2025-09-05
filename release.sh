#!/bin/sh
if [ "$(uname)" = Linux ]
then
    tlb_flags=""
    hugepages=$(grep HugePages_Free /proc/meminfo | awk '{print $2}')
    if [ "$hugepages" -gt 0 ]
    then
        tlb_flags="${tlb_flags} -DUSE_HUGETLB"
    fi
    CFLAGS='-O3 -D_GNU_SOURCE '"${tlb_flags}"' -flto -Wall' "${@}"
else
    CFLAGS='-O3 -flto -Wall' "${@}"
fi
ulimit -n 16384 # VK_FD_MAX
