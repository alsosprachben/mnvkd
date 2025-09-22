#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>

#include "vk_huge.h"
#include "vk_huge_s.h"
#include "vk_wrapguard.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void vk_huge_init(struct vk_huge* st)
{
    memset(st, 0, sizeof(*st));
    st->mode = VK_HUGE_AUTO;
    st->refresh_cooldown_ns = 1000ull * 1000 * 1000; /* 1s */
    st->fail_cooldown_ns = 2000ull * 1000 * 1000;    /* 2s */
    st->huge_page_bytes = 2u * 1024 * 1024;          /* default 2MB */
    st->last_refresh_ns = 0;
    st->last_fail_ns = 0;
    st->free_pages_cached = 0;
    st->flags = 0;
#if defined(MAP_HUGETLB)
    st->flags |= MAP_HUGETLB;
#  ifndef MAP_HUGE_2MB
#    define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#  endif
#  ifdef MAP_HUGE_2MB
    st->flags |= MAP_HUGE_2MB;
#  endif
#endif
}

void vk_huge_set_mode(struct vk_huge* st, enum vk_huge_mode mode)
{
    st->mode = mode;
}

enum vk_huge_mode vk_huge_get_mode(struct vk_huge* st) { return (enum vk_huge_mode)st->mode; }

static void refresh_from_proc(struct vk_huge* st)
{
#ifdef __linux__
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[256];
    size_t huge_kb = 0, free_pages = 0;
    while (fgets(line, sizeof(line), f)) {
        /* parse Hugepagesize: <n> kB */
        if (!huge_kb && strncmp(line, "Hugepagesize:", 13) == 0) {
            char* p = line + 13;
            while (*p == ' ' || *p == '\t') p++;
            huge_kb = strtoul(p, NULL, 10);
            continue;
        }
        /* parse HugePages_Free: <n> */
        if (!free_pages && strncmp(line, "HugePages_Free:", 15) == 0) {
            char* p = line + 15;
            while (*p == ' ' || *p == '\t') p++;
            free_pages = strtoul(p, NULL, 10);
            continue;
        }
    }
    fclose(f);
    if (huge_kb > 0) {
        st->huge_page_bytes = huge_kb * 1024u;
    }
    if (free_pages > 0) {
        st->free_pages_cached = free_pages;
    } else {
        /* keep last positive value; zero is a strong negative signal */
        st->free_pages_cached = 0;
    }
#else
    (void)st;
#endif
}

void vk_huge_refresh(struct vk_huge* st)
{
    uint64_t t = now_ns();
    if (t - st->last_refresh_ns < st->refresh_cooldown_ns) return;
    refresh_from_proc(st);
    st->last_refresh_ns = t;
}

int vk_huge_should_try(struct vk_huge* st, size_t len_bytes)
{
#if !defined(MAP_HUGETLB)
    (void)st; (void)len_bytes; return 0;
#else
    if (st->mode == VK_HUGE_NEVER) return 0;
    if (st->flags == 0) return 0;
    uint64_t t = now_ns();
    if (st->last_fail_ns && (t - st->last_fail_ns) < st->fail_cooldown_ns) return 0;
    if (st->mode == VK_HUGE_ALWAYS) return 1;
    /* AUTO: refresh and compare budget */
    vk_huge_refresh(st);
    size_t page = st->huge_page_bytes ? st->huge_page_bytes : (2u * 1024 * 1024);
    size_t needed = (len_bytes + page - 1) / page;
    return st->free_pages_cached >= needed;
#endif
}

int vk_huge_flags(struct vk_huge* st)
{
    return st->flags;
}

void vk_huge_on_success(struct vk_huge* st, size_t len_bytes)
{
    (void)len_bytes;
    st->last_fail_ns = 0;
    /* Optionally decrement budget heuristically */
    if (st->free_pages_cached > 0 && st->huge_page_bytes > 0) {
        size_t page = st->huge_page_bytes;
        size_t used = (len_bytes + page - 1) / page;
        if (used <= st->free_pages_cached) st->free_pages_cached -= used;
        else st->free_pages_cached = 0;
    }
}

void vk_huge_on_failure(struct vk_huge* st, int err)
{
    (void)err;
    st->last_fail_ns = now_ns();
    /* pessimistic: assume zero free until next refresh */
    st->free_pages_cached = 0;
}

size_t vk_huge_page_bytes(struct vk_huge* st)
{
    return st && st->huge_page_bytes ? st->huge_page_bytes : (size_t)(2u * 1024 * 1024);
}

int vk_huge_aligned_len(struct vk_huge* st, size_t nmemb, size_t count, size_t* out_len)
{
    size_t len;
    int rc;
    size_t page = vk_huge_page_bytes(st);
    if (page == 0) {
        errno = EINVAL;
        return -1;
    }

    rc = vk_safe_mult(nmemb, count, &len);
    if (rc == -1) return -1;

    /* ceil(len / page) * page with overflow guards */
    size_t pages = len / page;
    if (len % page) {
        if (vk_safe_add(pages, 1, &pages) == -1) return -1;
    }
    return vk_safe_mult(pages, page, out_len);
}
