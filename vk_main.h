#ifndef VK_MAIN_H
#define VK_MAIN_H

#include "vk_thread.h"

int vk_main_init(vk_func main_vk, void *arg_buf, size_t arg_len, size_t page_count, int privileged, int isolated);

#endif
