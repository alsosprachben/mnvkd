#include "vk_server.h"
#include "vk_server_s.h"
#include "vk_socket.h"

size_t vk_server_alloc_size() {
    return sizeof (struct vk_server);
}

void vk_server_set_address(struct vk_server *server_ptr, struct sockaddr *address_ptr, socklen_t address_len) {
    server_ptr->address = address_ptr;
    server_ptr->address_len  = address_len;
}

struct sockaddr *vk_server_get_address(struct vk_server *server_ptr) {
    return server_ptr->address;
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

	rc = bind(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), server_ptr->address, server_ptr->address_len);
	if (rc == -1) {
		return -1;
	}

	rc = listen(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), server_ptr->backlog);
	if (rc == -1) {
		return -1;
	}

    return 0;
}