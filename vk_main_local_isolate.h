#ifndef VK_MAIN_LOCAL_ISOLATE_H
#define VK_MAIN_LOCAL_ISOLATE_H

#include "vk_thread.h"
#include "vk_isolate.h"

/* Run a single local coroutine inside an isolated window.
 * The caller owns `iso` and should configure its scheduler callback and
 * privileged regions before calling this. */
int vk_main_local_isolate_init(vk_isolate_t *iso,
                               vk_func main_vk,
                               void *arg_buf,
                               size_t arg_len,
                               size_t page_count);

#endif

