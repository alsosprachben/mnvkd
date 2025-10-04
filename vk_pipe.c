/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_pipe_s.h"
#include "vk_socket.h"
#include "vk_vectoring.h"

static unsigned
vk_pipe_caps_for_fd_type(enum vk_fd_type fd_type)
{
	switch (fd_type) {
		case VK_FD_TYPE_SOCKET_STREAM:
		case VK_FD_TYPE_SOCKET_DATAGRAM:
		case VK_FD_TYPE_SOCKET_LISTEN:
			return VK_PIPE_CAP_SHUTDOWN_RX | VK_PIPE_CAP_SHUTDOWN_TX;
		default:
			return VK_PIPE_CAP_NONE;
	}
}

void vk_pipe_init_fd(struct vk_pipe* pipe_ptr, int fd, enum vk_fd_type fd_type)
{
	pipe_ptr->type = VK_PIPE_OS_FD;
	pipe_ptr->fd_type = (enum vk_fd_type)fd_type;
	pipe_ptr->ref.fd = fd;
	pipe_ptr->closed = 0;
	pipe_ptr->caps = vk_pipe_caps_for_fd_type(fd_type);
}
void vk_pipe_init_rx(struct vk_pipe* pipe_ptr, struct vk_socket* socket_ptr)
{
	pipe_ptr->type = VK_PIPE_VK_RX;
	pipe_ptr->fd_type = VK_FD_TYPE_UNKNOWN;
	pipe_ptr->ref.socket_ptr = socket_ptr;
	pipe_ptr->closed = 0;
	pipe_ptr->caps = VK_PIPE_CAP_SHUTDOWN_RX | VK_PIPE_CAP_SHUTDOWN_TX;
}
void vk_pipe_init_tx(struct vk_pipe* pipe_ptr, struct vk_socket* socket_ptr)
{
	pipe_ptr->type = VK_PIPE_VK_TX;
	pipe_ptr->fd_type = VK_FD_TYPE_UNKNOWN;
	pipe_ptr->ref.socket_ptr = socket_ptr;
	pipe_ptr->closed = 0;
	pipe_ptr->caps = VK_PIPE_CAP_SHUTDOWN_RX | VK_PIPE_CAP_SHUTDOWN_TX;
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
void vk_pipe_set_fd_type(struct vk_pipe* pipe_ptr, enum vk_fd_type fd_type)
{
	pipe_ptr->fd_type = fd_type;
	if (pipe_ptr->type == VK_PIPE_OS_FD) {
		pipe_ptr->caps = vk_pipe_caps_for_fd_type(fd_type);
	}
}

int vk_pipe_get_closed(struct vk_pipe* pipe_ptr) { return pipe_ptr->closed; }
void vk_pipe_set_closed(struct vk_pipe* pipe_ptr, int closed) { pipe_ptr->closed = closed; }

unsigned vk_pipe_get_caps(struct vk_pipe* pipe_ptr) { return pipe_ptr->caps; }
void vk_pipe_set_caps(struct vk_pipe* pipe_ptr, unsigned caps) { pipe_ptr->caps = caps; }
int vk_pipe_has_cap(struct vk_pipe* pipe_ptr, unsigned cap) { return (pipe_ptr->caps & cap) == cap; }
int vk_pipe_supports_shutdown_rx(struct vk_pipe* pipe_ptr)
{
	return vk_pipe_has_cap(pipe_ptr, VK_PIPE_CAP_SHUTDOWN_RX);
}
int vk_pipe_supports_shutdown_tx(struct vk_pipe* pipe_ptr)
{
	return vk_pipe_has_cap(pipe_ptr, VK_PIPE_CAP_SHUTDOWN_TX);
}
