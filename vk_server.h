#ifndef VK_SERVER_H
#define VK_SERVER_H

#include <sys/socket.h>
#include "vk_thread.h"

struct vk_server;
struct vk_socket;
size_t vk_server_alloc_size();

void vk_server_set_address(struct vk_server *server_ptr, struct sockaddr *address_ptr, socklen_t address_len);
struct sockaddr *vk_server_get_address(struct vk_server *server_ptr);
socklen_t vk_server_get_address_storage_len(struct vk_server *server_ptr);

socklen_t vk_server_get_address_len(struct vk_server *server_ptr);
void vk_server_set_address_len(struct vk_server *server_ptr, socklen_t address_len);
socklen_t *vk_server_get_address_len_ptr(struct vk_server *server_ptr);

char *vk_server_get_address_str(struct vk_server *server_ptr);
const char *vk_server_set_address_str(struct vk_server *server_ptr);
size_t vk_server_get_address_strlen(struct vk_server *server_ptr);

char *vk_server_get_port_str(struct vk_server *server_ptr);
int vk_server_set_port_str(struct vk_server *server_ptr);
size_t vk_server_get_port_strlen(struct vk_server *server_ptr);

void vk_server_set_socket(struct vk_server *server_ptr, int domain, int type, int protocol);
int vk_server_get_socket_domain(struct vk_server *server_ptr);
int vk_server_get_socket_type(struct vk_server *server_ptr);
int vk_server_get_socket_protocol(struct vk_server *server_ptr);

void vk_server_set_backlog(struct vk_server *server_ptr, int backlog);
int vk_server_get_backlog(struct vk_server *server_ptr);

vk_func vk_server_get_vk_func(struct vk_server *server_ptr);
void vk_server_set_vk_func(struct vk_server *server_ptr, vk_func vk_func);

size_t vk_server_get_page_count(struct vk_server *server_ptr);
void vk_server_set_page_count(struct vk_server *server_ptr, size_t page_count);

void *vk_server_get_msg(struct vk_server *server_ptr);
void vk_server_set_msg(struct vk_server *server_ptr, void *msg);

int vk_server_socket_listen(struct vk_server *server_ptr, struct vk_socket *socket_ptr);

#define vk_server_listen(server_ptr) vk_server_socket_listen(server_ptr, vk_get_socket(that))

int vk_server_init(struct vk_server *server_ptr);

#endif