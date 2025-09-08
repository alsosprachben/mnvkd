#ifndef VK_HUGE_S_H
#define VK_HUGE_S_H

#include <stddef.h>
#include <stdint.h>

struct vk_huge {
    int mode;                 /* enum vk_huge_mode */
    size_t huge_page_bytes;   /* e.g., 2MB on most systems */
    size_t free_pages_cached; /* last observed HugePages_Free */
    uint64_t last_refresh_ns; /* CLOCK_MONOTONIC */
    uint64_t last_fail_ns;    /* for cooldown */
    uint64_t refresh_cooldown_ns; /* min interval between /proc reads */
    uint64_t fail_cooldown_ns;    /* min interval after a failure */
    int flags;                /* mmap flags to request huge pages */
};

#endif

