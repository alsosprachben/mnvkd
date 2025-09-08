#ifndef VK_HUGE_H
#define VK_HUGE_H

#include <stddef.h>

/* Opaque tracker type */
struct vk_huge;

enum vk_huge_mode {
    VK_HUGE_NEVER = 0, /* never attempt MAP_HUGETLB */
    VK_HUGE_AUTO  = 1, /* attempt based on availability + cooldown */
    VK_HUGE_ALWAYS= 2  /* always attempt once per map (caller still falls back) */
};

void vk_huge_init(struct vk_huge* st);
void vk_huge_set_mode(struct vk_huge* st, enum vk_huge_mode mode);
enum vk_huge_mode vk_huge_get_mode(struct vk_huge* st);

/* Refresh cached availability (reads /proc/meminfo on Linux when cooldown elapsed). */
void vk_huge_refresh(struct vk_huge* st);

/* Whether it is worth attempting MAP_HUGETLB for a mapping of len_bytes. */
int vk_huge_should_try(struct vk_huge* st, size_t len_bytes);

/* Flags to OR into mmap() when attempting huge pages. */
int vk_huge_flags(struct vk_huge* st);

/* Notify tracker about outcome to adjust heuristics. */
void vk_huge_on_success(struct vk_huge* st, size_t len_bytes);
void vk_huge_on_failure(struct vk_huge* st, int err);

/* Helpers for HUGETLB alignment using the tracked huge page size. */
size_t vk_huge_page_bytes(struct vk_huge* st);
int vk_huge_aligned_len(struct vk_huge* st, size_t nmemb, size_t count, size_t* out_len);

#endif
