#ifndef VK_FETCH_H
#define VK_FETCH_H

#include "vk_thread.h"

/* Function prototypes for the fetch client */
void vk_fetch_request(struct vk_thread *that);
void vk_fetch_response(struct vk_thread *that);

#endif /* VK_FETCH_H */