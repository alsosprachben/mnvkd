/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_accepted.h"
#include "vk_accepted_s.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#define vk_portable_accept(fd, addr, addrlen, flags) accept4(fd, addr, addrlen, flags)
#define vk_portable_nonblock(fd) (void)0
#else
#include <sys/socket.h>
#include <fcntl.h>
#define vk_portable_accept(fd, addr, addrlen, flags) accept(fd, addr, addrlen)
#define vk_portable_nonblock(fd) fcntl(fd, F_SETFL, O_NONBLOCK)
#endif

int vk_accepted_accept(struct vk_accepted *accepted_ptr, int listen_fd) {
    int accepted_fd;
    const char *address_str;
    const char *port_str;

    memset(accepted_ptr, 0, vk_accepted_alloc_size());

    *vk_accepted_get_address_len_ptr(accepted_ptr) = vk_accepted_get_address_storage_len();
    accepted_fd = vk_portable_accept(listen_fd, vk_accepted_get_address(accepted_ptr), vk_accepted_get_address_len_ptr(accepted_ptr), SOCK_NONBLOCK);
    if (accepted_fd == -1) {
        return -1;
    }

    address_str = vk_accepted_set_address_str(accepted_ptr);
    if (address_str == NULL) {
        return -1;
    }
    port_str = vk_accepted_set_port_str(accepted_ptr);
    if (port_str == NULL) {
        return -1;
    }

    vk_portable_nonblock(accepted_fd);

    accepted_ptr->fd = accepted_fd;

    return accepted_fd;
}

size_t vk_accepted_alloc_size() {
    return sizeof (struct vk_accepted);
}

int vk_accepted_get_fd(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->fd;
}

void vk_accepted_set_address(struct vk_accepted *accepted_ptr, struct sockaddr *address_ptr, socklen_t address_len) {
    memcpy(&accepted_ptr->address, address_ptr, address_len);
    accepted_ptr->address_len  = address_len;
}
struct sockaddr *vk_accepted_get_address(struct vk_accepted *accepted_ptr) {
    return (struct sockaddr *) &accepted_ptr->address;
}
socklen_t vk_accepted_get_address_storage_len() {
    return sizeof (struct sockaddr_storage);
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
        	return inet_ntop(AF_INET,  &((struct sockaddr_in  *) vk_accepted_get_address(accepted_ptr))->sin_addr,  vk_accepted_get_address_str(accepted_ptr), vk_accepted_get_address_strlen());
        case AF_INET6:
        	return inet_ntop(AF_INET6, &((struct sockaddr_in6 *) vk_accepted_get_address(accepted_ptr))->sin6_addr, vk_accepted_get_address_str(accepted_ptr), vk_accepted_get_address_strlen());
        default:
            errno = ENOTSUP;
            return NULL;
    }
}
size_t vk_accepted_get_address_strlen() {
    return INET6_ADDRSTRLEN + 1;
}

const char *vk_accepted_get_port_str(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->port_str;
}
const char *vk_accepted_set_port_str(struct vk_accepted *accepted_ptr) {
    int rc;
    switch (vk_accepted_get_address(accepted_ptr)->sa_family) {
        case AF_INET:
            rc = snprintf(accepted_ptr->port_str, sizeof (accepted_ptr->port_str), "%i", (int) htons(((struct sockaddr_in  *) vk_accepted_get_address(accepted_ptr))->sin_port));
            break;
        case AF_INET6:
            rc = snprintf(accepted_ptr->port_str, sizeof (accepted_ptr->port_str), "%i", (int) htons(((struct sockaddr_in6 *) vk_accepted_get_address(accepted_ptr))->sin6_port));
            break;
        default:
            errno = ENOTSUP;
            rc = -1;
    }
    if (rc == -1) {
        return NULL;
    }
    return accepted_ptr->port_str;
}
size_t vk_accepted_get_port_strlen() {
    return 6;
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

