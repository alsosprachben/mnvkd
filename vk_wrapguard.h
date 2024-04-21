#ifndef VK_WRAPGUARD_H
#define VK_WRAPGUARD_H

#include <stddef.h>

size_t vk_pagesize();
int vk_wrapguard_mult(size_t a, size_t b);
int vk_wrapguard_add(size_t a, size_t b);
int vk_safe_mult(size_t a, size_t b, size_t* c_ptr);
int vk_safe_add(size_t a, size_t b, size_t* c_ptr);
int vk_safe_pagelen(size_t nmemb, size_t count, size_t* pagelen_ptr);
int vk_safe_hugepagelen(size_t nmemb, size_t count, size_t* pagelen_ptr);
int vk_safe_alignedlen(size_t nmemb, size_t count, size_t* alignedlen_ptr);
int vk_safe_hugealignedlen(size_t nmemb, size_t count, size_t* alignedlen_ptr);

#endif
