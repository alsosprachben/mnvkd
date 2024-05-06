#ifndef VK_MAIN_LOCAL_H
#define VK_MAIN_LOCAL_H

#include "vk_thread.h"

int vk_local_main_init(vk_func main_vk, void *arg_buf, size_t arg_len, size_t page_count);

#endif