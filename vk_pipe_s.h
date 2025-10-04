#ifndef VK_PIPE_S_H
#define VK_PIPE_S_H

#include "vk_fd_e.h"
#include "vk_pipe.h"

/*
 * `struct vk_pipe`: Represents the references to a single direction of a `struct vk_socket`.
 *
 * The `struct vk_pipe_type` branches the `union vk_pipe_ref`, and for `socket_ptr`, which side of the socket (and
 * therefore which underlying pipe), to point to. This allows a chain of different types of pipes, connecting code and
 * physical resources. For example:
 *  - Two physical pipes (a readable and a writable) may be connected to a chain of `struct vk_socket` intermediate
 * pipes.
 *
 * The `struct vk_fd_type` is used on physical pipes to determine the physical operations. But here, it determines which
 * protocol to use for blocking ops. For example:
 *  - A listen socket is a readable FD that needs an `accept()` that produces a `struct vk_accepted` for each call.
 *  - There may be a chain of `struct vk_socket` that forward these `struct vk_accepted` messages. Each blocking op
 * needs to be aware of this messaging protocol, so those `struct vk_pipe::fd_type` will be marked with
 * `VK_FD_TYPE_SOCKET_LISTEN`, not just the originating `vk_fd.type`.
 */

union vk_pipe_ref {
	int fd;
	struct vk_socket* socket_ptr;
};
struct vk_pipe {
	enum vk_pipe_type type;
	enum vk_fd_type fd_type; /* at this level, for determining protocol for blocking ops */
	union vk_pipe_ref ref;
	int closed;
	unsigned caps;
};

#endif
