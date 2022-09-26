/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_accepted.h"
#include "vk_accepted_s.h"


#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

size_t vk_accepted_alloc_size() {
    return sizeof (struct vk_accepted);
}

void vk_accepted_set_address(struct vk_accepted *accepted_ptr, struct sockaddr *address_ptr, socklen_t address_len) {
    memcpy(&accepted_ptr->address, address_ptr, address_len);
    accepted_ptr->address_len  = address_len;
}
struct sockaddr *vk_accepted_get_address(struct vk_accepted *accepted_ptr) {
    return (struct sockaddr *) &accepted_ptr->address;
}
socklen_t vk_accepted_get_address_storage_len(struct vk_accepted *accepted_ptr) {
    return sizeof (accepted_ptr->address);
}

socklen_t vk_accepted_get_address_len(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->address_len;
}
void vk_accepted_set_address_len(struct vk_accepted *accepted_ptr, socklen_t address_len) {
    accepted_ptr->address_len = address_len;
}
socklen_t *vk_accepted_get_address_len_ptr(struct vk_accepted *accepted_ptr) {
    return &accepted_ptr->address_len;
}

char *vk_accepted_get_address_str(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->address_str;
}
const char *vk_accepted_set_address_str(struct vk_accepted *accepted_ptr) {
    switch (vk_accepted_get_address(accepted_ptr)->sa_family) {
        case AF_INET:
        	return inet_ntop(AF_INET,  &((struct sockaddr_in  *) vk_accepted_get_address(accepted_ptr))->sin_addr,  vk_accepted_get_address_str(accepted_ptr), vk_accepted_get_address_strlen(accepted_ptr));
        case AF_INET6:
        	return inet_ntop(AF_INET6, &((struct sockaddr_in6 *) vk_accepted_get_address(accepted_ptr))->sin6_addr, vk_accepted_get_address_str(accepted_ptr), vk_accepted_get_address_strlen(accepted_ptr));
        default:
            errno = ENOTSUP;
            return NULL;
    }
}
size_t vk_accepted_get_address_strlen(struct vk_accepted *accepted_ptr) {
    return sizeof (accepted_ptr->address_str);
}

char *vk_accepted_get_port_str(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->port_str;
}
int vk_accepted_set_port_str(struct vk_accepted *accepted_ptr) {
    switch (vk_accepted_get_address(accepted_ptr)->sa_family) {
        case AF_INET:
            return snprintf(accepted_ptr->port_str, sizeof (accepted_ptr->port_str), "%i", (int) htons(((struct sockaddr_in  *) vk_accepted_get_address(accepted_ptr))->sin_port));
        case AF_INET6:
            return snprintf(accepted_ptr->port_str, sizeof (accepted_ptr->port_str), "%i", (int) htons(((struct sockaddr_in6 *) vk_accepted_get_address(accepted_ptr))->sin6_port));
        default:
            errno = ENOTSUP;
            return -1;
    }
}
size_t vk_accepted_get_port_strlen(struct vk_accepted *accepted_ptr) {
    return sizeof (accepted_ptr->port_str);
}

struct vk_thread *vk_accepted_get_vk(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->vk_ptr;
}
void vk_accepted_set_vk(struct vk_accepted *accepted_ptr, struct vk_thread *that) {
    accepted_ptr->vk_ptr = that;
}

struct vk_proc *vk_accepted_get_proc(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->proc_ptr;
}
void vk_accepted_set_proc(struct vk_accepted *accepted_ptr, struct vk_proc *proc_ptr) {
    accepted_ptr->proc_ptr = proc_ptr;
}
