#include "vk_server.h"
#include "vk_server_s.h"
#include "vk_socket.h"

#include <unistd.h>
#include <sys/socket.h>
#include <string.h>

size_t vk_server_alloc_size() {
    return sizeof (struct vk_server);
}

void vk_server_set_address(struct vk_server *server_ptr, struct sockaddr *address_ptr, socklen_t address_len) {
    memcpy(&server_ptr->address, address_ptr, address_len);
    server_ptr->address_len  = address_len;
}
struct sockaddr *vk_server_get_address(struct vk_server *server_ptr) {
    return (struct sockaddr *) &server_ptr->address;
}
socklen_t vk_server_get_address_storage_len(struct vk_server *server_ptr) {
    return sizeof (server_ptr->address);
}

socklen_t vk_server_get_address_len(struct vk_server *server_ptr) {
    return server_ptr->address_len;
}
void vk_server_set_address_len(struct vk_server *server_ptr, socklen_t address_len) {
    server_ptr->address_len = address_len;
}
socklen_t *vk_server_get_address_len_ptr(struct vk_server *server_ptr) {
    return &server_ptr->address_len;
}

char *vk_server_get_address_str(struct vk_server *server_ptr) {
    return server_ptr->address_str;
}
const char *vk_server_set_address_str(struct vk_server *server_ptr) {
    switch (vk_server_get_address(server_ptr)->sa_family) {
        case AF_INET:
        	return inet_ntop(AF_INET,  &((struct sockaddr_in  *) vk_server_get_address(server_ptr))->sin_addr,  vk_server_get_address_str(server_ptr), vk_server_get_address_strlen(server_ptr));
        case AF_INET6:
        	return inet_ntop(AF_INET6, &((struct sockaddr_in6 *) vk_server_get_address(server_ptr))->sin6_addr, vk_server_get_address_str(server_ptr), vk_server_get_address_strlen(server_ptr));
        default:
            errno = ENOTSUP;
            return NULL;
    }
}
size_t vk_server_get_address_strlen(struct vk_server *server_ptr) {
    return sizeof (server_ptr->address_str);
}

void vk_server_set_socket(struct vk_server *server_ptr, int domain, int type, int protocol) {
    server_ptr->domain = domain;
    server_ptr->type = type;
    server_ptr->protocol = protocol;
}

int vk_server_get_socket_domain(struct vk_server *server_ptr) {
    return server_ptr->domain;
}
int vk_server_get_socket_type(struct vk_server *server_ptr) {
    return server_ptr->type;
}
int vk_server_get_socket_protocol(struct vk_server *server_ptr) {
    return server_ptr->protocol;
}

void vk_server_set_backlog(struct vk_server *server_ptr, int backlog) {
    server_ptr->backlog = backlog;
}

int vk_server_get_backlog(struct vk_server *server_ptr) {
    return server_ptr->backlog;
}

vk_func vk_server_get_vk_func(struct vk_server *server_ptr) {
    return server_ptr->service_vk_func;
}
void vk_server_set_vk_func(struct vk_server *server_ptr, vk_func vk_func) {
    server_ptr->service_vk_func = vk_func;
}

void *vk_server_get_msg(struct vk_server *server_ptr) {
    return server_ptr->service_msg;
}
void vk_server_set_msg(struct vk_server *server_ptr, void *msg) {
    server_ptr->service_msg = msg;
}

int vk_server_socket_listen(struct vk_server *server_ptr, struct vk_socket *socket_ptr) {
    int rc;
    int opt;

	rc = socket(server_ptr->domain, server_ptr->type, server_ptr->protocol);
	if (rc == -1) {
		return -1;
	}
	vk_pipe_init_fd(vk_socket_get_rx_fd(socket_ptr), rc);

    opt = 1;
	rc = setsockopt(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
	if (rc == -1) {
		return -1;
	}

	rc = bind(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), (struct sockaddr *) &server_ptr->address, server_ptr->address_len);
	if (rc == -1) {
		return -1;
	}

	rc = listen(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), server_ptr->backlog);
	if (rc == -1) {
		return -1;
	}

    if (vk_server_set_address_str(server_ptr) == NULL) {
		return -1;
	}

    DBG("vk_server_socket_listen(%s)\n", vk_server_get_address_str(server_ptr));

    return 0;
}