#ifndef VK_SERVER_H
#define VK_SERVER_H

#include <sys/socket.h>

struct vk_server;
struct vk_socket;
size_t vk_server_alloc_size();
void vk_server_set_address(struct vk_server *server_ptr, struct sockaddr *address_ptr, socklen_t address_len);
void vk_server_set_socket(struct vk_server *server_ptr, int domain, int type, int protocol);
void vk_server_set_backlog(struct vk_server *server_ptr, int backlog);
int vk_server_socket_listen(struct vk_server *server_ptr, struct vk_socket *socket_ptr);

#define vk_server_listen(server_ptr) vk_server_socket_listen(server_ptr, vk_get_socket(that))

#endif