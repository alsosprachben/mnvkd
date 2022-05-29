#ifndef VK_PIPE_S_H
#define VK_PIPE_S_H

#include "vk_pipe.h"

union vk_pipe_ref {
	int fd;
	struct vk_socket *socket_ptr;
};
struct vk_pipe {
	enum vk_pipe_type type;
	union vk_pipe_ref ref;
};

#endif