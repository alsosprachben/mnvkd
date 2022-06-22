#ifndef VK_PIPE_H
#define VK_PIPE_H

enum vk_pipe_type {
	VK_PIPE_OS_FD,
	VK_PIPE_VK_RX,
	VK_PIPE_VK_TX,
};

union vk_pipe_ref;
struct vk_pipe;
struct vk_socket;

void vk_pipe_init_fd(struct vk_pipe *pipe_ptr, int fd);
void vk_pipe_init_rx(struct vk_pipe *pipe_ptr, struct vk_socket *socket_ptr);
void vk_pipe_init_tx(struct vk_pipe *pipe_ptr, struct vk_socket *socket_ptr);
int vk_pipe_get_fd(struct vk_pipe *pipe_ptr);
struct vk_vectoring *vk_pipe_get_rx(struct vk_pipe *pipe_ptr);
struct vk_vectoring *vk_pipe_get_tx(struct vk_pipe *pipe_ptr);
struct vk_socket *vk_pipe_get_socket(struct vk_pipe *pipe_ptr);

#endif