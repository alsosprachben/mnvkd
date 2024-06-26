/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_server.h"
#include "vk_fd_table.h"
#include "vk_kern.h"
#include "vk_pool.h"
#include "vk_proc.h"
#include "vk_server_s.h"
#include "vk_service.h"
#include "vk_socket.h"
#include "vk_wrapguard.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // Added for TCP_NODELAY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

size_t vk_server_alloc_size() { return sizeof(struct vk_server); }

struct vk_kern* vk_server_get_kern(struct vk_server* server_ptr) { return server_ptr->kern_ptr; }
void vk_server_set_kern(struct vk_server* server_ptr, struct vk_kern* kern_ptr) { server_ptr->kern_ptr = kern_ptr; }

struct vk_pool* vk_server_get_pool(struct vk_server* server_ptr) { return server_ptr->pool_ptr; }
void vk_server_set_pool(struct vk_server* server_ptr, struct vk_pool* pool_ptr) { server_ptr->pool_ptr = pool_ptr; }

void vk_server_set_address(struct vk_server* server_ptr, struct sockaddr* address_ptr, socklen_t address_len)
{
	memcpy(&server_ptr->address, address_ptr, address_len);
	server_ptr->address_len = address_len;
}
struct sockaddr* vk_server_get_address(struct vk_server* server_ptr) { return (struct sockaddr*)&server_ptr->address; }
socklen_t vk_server_get_address_storage_len(struct vk_server* server_ptr) { return sizeof(server_ptr->address); }

socklen_t vk_server_get_address_len(struct vk_server* server_ptr) { return server_ptr->address_len; }
void vk_server_set_address_len(struct vk_server* server_ptr, socklen_t address_len)
{
	server_ptr->address_len = address_len;
}
socklen_t* vk_server_get_address_len_ptr(struct vk_server* server_ptr) { return &server_ptr->address_len; }

char* vk_server_get_address_str(struct vk_server* server_ptr) { return server_ptr->address_str; }
const char* vk_server_set_address_str(struct vk_server* server_ptr)
{
	switch (vk_server_get_address(server_ptr)->sa_family) {
		case AF_INET:
			return inet_ntop(AF_INET, &((struct sockaddr_in*)vk_server_get_address(server_ptr))->sin_addr,
					 vk_server_get_address_str(server_ptr),
					 vk_server_get_address_strlen(server_ptr));
		case AF_INET6:
			return inet_ntop(
			    AF_INET6, &((struct sockaddr_in6*)vk_server_get_address(server_ptr))->sin6_addr,
			    vk_server_get_address_str(server_ptr), vk_server_get_address_strlen(server_ptr));
		default:
			errno = ENOTSUP;
			return NULL;
	}
}
size_t vk_server_get_address_strlen(struct vk_server* server_ptr) { return sizeof(server_ptr->address_str); }

char* vk_server_get_port_str(struct vk_server* server_ptr) { return server_ptr->port_str; }
int vk_server_set_port_str(struct vk_server* server_ptr)
{
	switch (vk_server_get_address(server_ptr)->sa_family) {
		case AF_INET:
			return snprintf(server_ptr->port_str, sizeof(server_ptr->port_str), "%i",
					(int)htons(((struct sockaddr_in*)vk_server_get_address(server_ptr))->sin_port));
		case AF_INET6:
			return snprintf(
			    server_ptr->port_str, sizeof(server_ptr->port_str), "%i",
			    (int)htons(((struct sockaddr_in6*)vk_server_get_address(server_ptr))->sin6_port));
		default:
			errno = ENOTSUP;
			return -1;
	}
}
size_t vk_server_get_port_strlen(struct vk_server* server_ptr) { return sizeof(server_ptr->port_str); }

void vk_server_set_socket(struct vk_server* server_ptr, int domain, int type, int protocol)
{
	server_ptr->domain = domain;
	server_ptr->type = type;
	server_ptr->protocol = protocol;
}

int vk_server_get_socket_domain(struct vk_server* server_ptr) { return server_ptr->domain; }
int vk_server_get_socket_type(struct vk_server* server_ptr) { return server_ptr->type; }
int vk_server_get_socket_protocol(struct vk_server* server_ptr) { return server_ptr->protocol; }

void vk_server_set_backlog(struct vk_server* server_ptr, int backlog) { server_ptr->backlog = backlog; }

int vk_server_get_backlog(struct vk_server* server_ptr) { return server_ptr->backlog; }

vk_func vk_server_get_vk_func(struct vk_server* server_ptr) { return server_ptr->service_vk_func; }
void vk_server_set_vk_func(struct vk_server* server_ptr, vk_func vk_func) { server_ptr->service_vk_func = vk_func; }

int vk_server_get_privileged(struct vk_server* server_ptr) { return server_ptr->privileged; }
void vk_server_set_privileged(struct vk_server* server_ptr, int privileged) { server_ptr->privileged = privileged; }

int vk_server_get_isolated(struct vk_server* server_ptr) { return server_ptr->isolated; }
void vk_server_set_isolated(struct vk_server* server_ptr, int isolated) { server_ptr->isolated = isolated; }

int vk_server_get_reuseport(struct vk_server* server_ptr) { return server_ptr->reuseport; }
void vk_server_set_reuseport(struct vk_server* server_ptr, int reuseport) { server_ptr->reuseport = reuseport; }

size_t vk_server_get_count(struct vk_server* server_ptr) { return server_ptr->service_count; }

void vk_server_set_count(struct vk_server* server_ptr, size_t count) { server_ptr->service_count = count; }

size_t vk_server_get_page_count(struct vk_server* server_ptr) { return server_ptr->service_page_count; }
void vk_server_set_page_count(struct vk_server* server_ptr, size_t page_count)
{
	server_ptr->service_page_count = page_count;
}

void* vk_server_get_msg(struct vk_server* server_ptr) { return server_ptr->service_msg; }
void vk_server_set_msg(struct vk_server* server_ptr, void* msg) { server_ptr->service_msg = msg; }

int vk_server_socket_connect(struct vk_server* server_ptr, struct vk_socket* socket_ptr)
{
	int rc;
	int opt;

	rc = socket(server_ptr->domain, server_ptr->type, server_ptr->protocol);
	if (rc == -1) {
		PERROR("listen socket");
		return -1;
	}
	vk_pipe_init_fd(vk_socket_get_rx_fd(socket_ptr), rc, VK_FD_TYPE_SOCKET_STREAM);

	rc = fcntl(rc, F_SETFL, O_NONBLOCK);
	if (rc == -1) {
		PERROR("listen socket nonblock fcntl");
		return -1;
	}

	opt = 1;
	rc = setsockopt(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
	if (rc == -1) {
		PERROR("listen socket setsockopt");
		return -1;
	}

	if (vk_server_get_reuseport(server_ptr)) {
		opt = 1;
		rc = setsockopt(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
		if (rc == -1) {
			PERROR("listen socket setsockopt");
			return -1;
		}
	}

	rc = connect(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), (struct sockaddr*)&server_ptr->address,
		     server_ptr->address_len);
	if (rc == -1) {
		PERROR("listen socket bind");
		return -1;
	}

	if (vk_server_set_address_str(server_ptr) == NULL) {
		return -1;
	}
	rc = vk_server_set_port_str(server_ptr);
	if (rc == -1) {
		return -1;
	}

	vk_socket_dbgf("vk_server_socket_connect(%s:%s)\n", vk_server_get_address_str(server_ptr),
		       vk_server_get_port_str(server_ptr));

	return 0;
}

int vk_server_socket_listen(struct vk_server* server_ptr, struct vk_socket* socket_ptr)
{
	int rc;
	int opt;

	rc = socket(server_ptr->domain, server_ptr->type, server_ptr->protocol);
	if (rc == -1) {
		PERROR("listen socket");
		return -1;
	}
	vk_pipe_init_fd(vk_socket_get_rx_fd(socket_ptr), rc, VK_FD_TYPE_SOCKET_LISTEN);

	rc = fcntl(rc, F_SETFL, O_NONBLOCK);
	if (rc == -1) {
		PERROR("listen socket nonblock fcntl");
		return -1;
	}

	opt = 1;
	rc = setsockopt(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (rc == -1) {
		PERROR("listen socket setsockopt");
		return -1;
	}

	rc = bind(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), (struct sockaddr*)&server_ptr->address,
		  server_ptr->address_len);
	if (rc == -1) {
		PERROR("listen socket bind");
		return -1;
	}

	rc = listen(vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), server_ptr->backlog);
	if (rc == -1) {
		PERROR("listen socket listen");
		return -1;
	}

	if (vk_server_set_address_str(server_ptr) == NULL) {
		return -1;
	}
	rc = vk_server_set_port_str(server_ptr);
	if (rc == -1) {
		return -1;
	}

	vk_socket_dbgf("vk_server_socket_listen(%s:%s)\n", vk_server_get_address_str(server_ptr),
		       vk_server_get_port_str(server_ptr));

	return 0;
}

int vk_server_init(struct vk_server* server_ptr)
{
	int rc;
	struct vk_heap* kern_heap_ptr;
	struct vk_kern* kern_ptr;
	struct vk_proc* proc_ptr;
	struct vk_thread* vk_ptr;
	char* vk_poll_driver;
	char* vk_poll_method;

	kern_heap_ptr = calloc(1, vk_heap_alloc_size());
	kern_ptr = vk_kern_alloc(kern_heap_ptr);
	if (kern_ptr == NULL) {
		return -1;
	}

	server_ptr->kern_ptr = kern_ptr;

	if (server_ptr->service_count > 0) {
		rc = vk_pool_init(server_ptr->pool_ptr, server_ptr->service_page_count * vk_pagesize(),
				  server_ptr->service_count, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0);
		if (rc == -1) {
			return -1;
		}
	}

	proc_ptr = vk_kern_alloc_proc(kern_ptr, NULL);
	if (proc_ptr == NULL) {
		return -1;
	}
	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * 34, 0, 1);
	if (rc == -1) {
		return -1;
	}
	vk_proc_set_privileged(proc_ptr, 1); /* needs access to kernel to spawn processes for accepted connections */
	vk_proc_set_isolated(proc_ptr, server_ptr->isolated);

	vk_ptr = vk_proc_alloc_thread(proc_ptr);
	if (vk_ptr == NULL) {
		return -1;
	}

	/*
	 * Initialize vk_service_listener with stdin and stdout bound to its default socket.
	 * vk_service_listener will later bind a listener to the read pipe, replacing stdin.
	 */
	fcntl(0, F_SETFL, O_NONBLOCK);
	fcntl(1, F_SETFL, O_NONBLOCK);

	VK_INIT(vk_ptr, vk_proc_get_local(proc_ptr), vk_service_listener, 0, VK_FD_TYPE_SOCKET_STREAM, 1,
		VK_FD_TYPE_SOCKET_STREAM);
	rc = vk_copy_arg(vk_ptr, server_ptr, vk_server_alloc_size());
	if (rc == -1) {
		PERROR("vk_copy_arg");
		return -1;
	}

	/* enqueue it to run */
	vk_proc_local_enqueue_run(vk_proc_get_local(proc_ptr), vk_ptr);
	/* flush run status to kernel */
	vk_kern_flush_proc_queues(kern_ptr, proc_ptr);

	vk_poll_driver = getenv("VK_POLL_DRIVER");
	if (vk_poll_driver != NULL && strcmp(vk_poll_driver, "OS") == 0) {
		vk_fd_table_set_poll_driver(vk_kern_get_fd_table(kern_ptr), VK_POLL_DRIVER_OS);
		ERR("Using OS-specific poller.\n");
	}
	vk_poll_method = getenv("VK_POLL_METHOD");
	if (vk_poll_method != NULL && strcmp(vk_poll_method, "EDGE_TRIGGERED") == 0) {
		vk_fd_table_set_poll_method(vk_kern_get_fd_table(kern_ptr), VK_POLL_METHOD_EDGE_TRIGGERED);
		ERR("Using edge-triggered polling.\n");
	}

	rc = vk_kern_loop(kern_ptr);
	if (rc == -1) {
		return -1;
	}

	rc = vk_proc_free(proc_ptr);
	if (rc == -1) {
		return -1;
	}

	vk_kern_free_proc(kern_ptr, proc_ptr);

	return 0;
}
