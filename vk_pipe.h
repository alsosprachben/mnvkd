#ifndef VK_PIPE_H
#define VK_PIPE_H

enum vk_pipe_type {
	VK_PIPE_OS_FD,
	VK_PIPE_VK_RX,
	VK_PIPE_VK_TX,
};

union vk_pipe_ref {
	int fd;
	struct vk_socket *socket_ptr;
};
struct vk_pipe {
	enum vk_pipe_type type;
	union vk_pipe_ref ref;
};
#define VK_PIPE_INIT_FD(pipe, fd_arg) { \
	(pipe).type = VK_PIPE_OS_FD; \
	(pipe).ref.fd = fd_arg; \
}
#define VK_PIPE_INIT_RX(pipe, socket_arg) { \
	(pipe).type = VK_PIPE_VK_RX; \
	(pipe).ref.socket_ptr = &(socket_arg); \
}
#define VK_PIPE_INIT_TX(pipe, socket_arg) { \
	(pipe).type = VK_PIPE_VK_TX; \
	(pipe).ref.socket_ptr = &(socket_arg); \
}

#define VK_PIPE_GET_FD(pipe) ((pipe).type == VK_PIPE_OS_FD ?  (pipe).ref.fd : -1)
#define VK_PIPE_GET_RX(pipe) ((pipe).type == VK_PIPE_VK_RX ? &(pipe).ref.socket_ptr->rx.ring : NULL)
#define VK_PIPE_GET_TX(pipe) ((pipe).type == VK_PIPE_VK_TX ? &(pipe).ref.socket_ptr->tx.ring : NULL)
#define VK_PIPE_GET_SOCKET(pipe) ((pipe).type != VK_PIPE_OS_FD ? (pipe).ref.socket_ptr : NULL)

#endif