#ifndef VK_STACK_H
#define VK_STACK_H

#include <stddef.h>

struct vk_stack;

void vk_stack_init(struct vk_stack* stack_ptr, void* addr, size_t len);

void* vk_stack_push(struct vk_stack* stack_ptr, size_t nmemb, size_t count);
int vk_stack_pop(struct vk_stack* stack_ptr);

void* vk_stack_get_start(struct vk_stack* stack_ptr);
void* vk_stack_get_cursor(struct vk_stack* stack_ptr);
void* vk_stack_get_stop(struct vk_stack* stack_ptr);
size_t vk_stack_get_free(struct vk_stack* stack_ptr);
size_t vk_stack_get_length(struct vk_stack* stack_ptr);

int vk_stack_push_stack(struct vk_stack* stack_ptr, struct vk_stack* new_stack_ptr, size_t nmemb, size_t count);
int vk_stack_push_stack_pages(struct vk_stack* stack_ptr, struct vk_stack* new_stack_ptr, size_t page_count);

int vk_stack_pagelen(size_t nmemb, size_t count, size_t* pagelen_ptr);
int vk_stack_alignedlen(size_t nmemb, size_t count, size_t* alignedlen_ptr);

#endif
