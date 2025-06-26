#ifndef VK_PIPE_H
#define VK_PIPE_H

#include "vk_fd_e.h"
#ifdef USE_TLS
#include <openssl/bio.h>
#endif

enum vk_pipe_type {
	VK_PIPE_OS_FD,
	VK_PIPE_VK_RX,
	VK_PIPE_VK_TX,
};

struct vk_pipe;
struct vk_socket;

void vk_pipe_init_fd(struct vk_pipe* pipe_ptr, int fd, enum vk_fd_type fd_type);
void vk_pipe_init_rx(struct vk_pipe* pipe_ptr, struct vk_socket* socket_ptr);
void vk_pipe_init_tx(struct vk_pipe* pipe_ptr, struct vk_socket* socket_ptr);
int vk_pipe_get_fd(struct vk_pipe* pipe_ptr);
struct vk_vectoring* vk_pipe_get_rx(struct vk_pipe* pipe_ptr);
struct vk_vectoring* vk_pipe_get_tx(struct vk_pipe* pipe_ptr);
struct vk_socket* vk_pipe_get_socket(struct vk_pipe* pipe_ptr);
enum vk_pipe_type vk_pipe_get_type(struct vk_pipe* pipe_ptr);
void vk_pipe_set_type(struct vk_pipe* pipe_ptr, enum vk_pipe_type type);
enum vk_fd_type vk_pipe_get_fd_type(struct vk_pipe* pipe_ptr);
void vk_pipe_set_fd_type(struct vk_pipe* pipe_ptr, enum vk_fd_type fd_type);
int vk_pipe_get_closed(struct vk_pipe* pipe_ptr);
void vk_pipe_set_closed(struct vk_pipe* pipe_ptr, int closed);
#ifdef USE_TLS
int vk_pipe_init_tls(struct vk_pipe* pipe_ptr);
int vk_pipe_deinit_tls(struct vk_pipe* pipe_ptr);
BIO* vk_pipe_get_bio(struct vk_pipe* pipe_ptr);
void vk_pipe_set_bio(struct vk_pipe* pipe_ptr, BIO* bio);
#endif

#endif
