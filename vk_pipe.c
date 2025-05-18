/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_pipe_s.h"
#include "vk_socket.h"
#include "vk_vectoring.h"
#ifdef USE_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

void vk_pipe_init_fd(struct vk_pipe* pipe_ptr, int fd, enum vk_fd_type fd_type)
{
	pipe_ptr->type = VK_PIPE_OS_FD;
	pipe_ptr->fd_type = (enum vk_fd_type)fd_type;
	pipe_ptr->ref.fd = fd;
	pipe_ptr->closed = 0;
}
void vk_pipe_init_rx(struct vk_pipe* pipe_ptr, struct vk_socket* socket_ptr)
{
	pipe_ptr->type = VK_PIPE_VK_RX;
	pipe_ptr->ref.socket_ptr = socket_ptr;
	pipe_ptr->closed = 0;
}
void vk_pipe_init_tx(struct vk_pipe* pipe_ptr, struct vk_socket* socket_ptr)
{
	pipe_ptr->type = VK_PIPE_VK_TX;
	pipe_ptr->ref.socket_ptr = socket_ptr;
	pipe_ptr->closed = 0;
}
int vk_pipe_get_fd(struct vk_pipe* pipe_ptr) { return pipe_ptr->type == VK_PIPE_OS_FD ? pipe_ptr->ref.fd : -1; }
struct vk_vectoring* vk_pipe_get_rx(struct vk_pipe* pipe_ptr)
{
	switch (pipe_ptr->type) {
		case VK_PIPE_VK_RX:
			if (pipe_ptr->ref.socket_ptr != NULL) {
				return vk_socket_get_rx_vectoring(pipe_ptr->ref.socket_ptr);
			} else {
				return NULL;
			}
			break;
		case VK_PIPE_VK_TX:
		case VK_PIPE_OS_FD:
		default:
			return NULL;
	}
}
struct vk_vectoring* vk_pipe_get_tx(struct vk_pipe* pipe_ptr)
{
	switch (pipe_ptr->type) {
		case VK_PIPE_VK_TX:
			if (pipe_ptr->ref.socket_ptr != NULL) {
				return vk_socket_get_tx_vectoring(pipe_ptr->ref.socket_ptr);
			} else {
				return NULL;
			}
			break;
		case VK_PIPE_VK_RX:
		case VK_PIPE_OS_FD:
		default:
			return NULL;
	}
}

struct vk_socket* vk_pipe_get_socket(struct vk_pipe* pipe_ptr)
{
	switch (pipe_ptr->type) {
		case VK_PIPE_VK_RX:
		case VK_PIPE_VK_TX:
			return pipe_ptr->ref.socket_ptr;
		case VK_PIPE_OS_FD:
		default:
			return NULL;
	}
}

enum vk_pipe_type vk_pipe_get_type(struct vk_pipe* pipe_ptr) { return pipe_ptr->type; }
void vk_pipe_set_type(struct vk_pipe* pipe_ptr, enum vk_pipe_type type) { pipe_ptr->type = type; }

enum vk_fd_type vk_pipe_get_fd_type(struct vk_pipe* pipe_ptr) { return pipe_ptr->fd_type; }
void vk_pipe_set_fd_type(struct vk_pipe* pipe_ptr, enum vk_fd_type fd_type) { pipe_ptr->fd_type = fd_type; }

int vk_pipe_get_closed(struct vk_pipe* pipe_ptr) { return pipe_ptr->closed; }
void vk_pipe_set_closed(struct vk_pipe* pipe_ptr, int closed) { pipe_ptr->closed = closed; }

#ifdef USE_TLS
int vk_pipe_init_tls(struct vk_pipe* pipe_ptr)
{
	if (pipe_ptr->bio != NULL) {
		errno = EINVAL; /* double init */
		return -1;
	}
	switch (pipe_ptr->type) {
		VK_PIPE_OS_FD:
			pipe_ptr->bio = BIO_new_fd(vk_pipe_get_fd(pipe_ptr), BIO_NOCLOSE);
			break;
		VK_PIPE_VK_RX:
		VK_PIPE_VK_TX:
			pipe_ptr->bio = BIO_new(NULL);
			break;
	}
	if (pipe_ptr->bio == NULL) {
		ERR_print_errors_fp(stderr);
		return -1;
	}
	return 0;
}

int vk_pipe_deinit_tls(struct vk_pipe* pipe_ptr)
{
	if (pipe_ptr->bio == NULL) {
		errno = EINVAL; /* double free */
		return -1;
	}
	BIO_free(pipe_ptr->bio);
	pipe_ptr->bio = NULL;

	return 0;
}


#endif