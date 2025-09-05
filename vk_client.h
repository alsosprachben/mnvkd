#ifndef VK_CLIENT_H
#define VK_CLIENT_H

#include "vk_thread.h"

struct vk_client;
size_t vk_client_alloc_size();

const char* vk_client_get_address(struct vk_client* client_ptr);
void vk_client_set_address(struct vk_client* client_ptr, const char* address);
const char* vk_client_get_port(struct vk_client* client_ptr);
void vk_client_set_port(struct vk_client* client_ptr, const char* port);

/* request (stdin -> socket) */
vk_func vk_client_get_vk_req_func(struct vk_client* client_ptr);
void vk_client_set_vk_req_func(struct vk_client* client_ptr, vk_func func);
/* response (socket -> stdout) */
vk_func vk_client_get_vk_resp_func(struct vk_client* client_ptr);
void vk_client_set_vk_resp_func(struct vk_client* client_ptr, vk_func func);

/* Backward-compat: treat vk_func as request func */
static inline vk_func vk_client_get_vk_func(struct vk_client* client_ptr) { return vk_client_get_vk_req_func(client_ptr); }
static inline void vk_client_set_vk_func(struct vk_client* client_ptr, vk_func func) { vk_client_set_vk_req_func(client_ptr, func); }

int vk_client_init(struct vk_client* client_ptr, size_t page_count, int privileged, int isolated);

#endif
