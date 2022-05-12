#include "vk_poll_s.h"
#include "vk_state.h"
#include "debug.h"

void io_future_init(struct io_future *ioft, struct vk_socket *socket_ptr) {
	ioft->socket_ptr = socket_ptr;
	ioft->proc_ptr = vk_get_proc(socket_ptr->block.blocked_vk);
	switch (socket_ptr->block.op) {
		case VK_OP_READ:
			ioft->event.fd = VK_PIPE_GET_FD(socket_ptr->rx_fd);
			ioft->event.events = POLLIN;
			ioft->event.revents = 0;
			break;
		case VK_OP_WRITE:
		case VK_OP_FLUSH:
			ioft->event.fd = VK_PIPE_GET_FD(socket_ptr->tx_fd);
			ioft->event.events = POLLOUT;
			ioft->event.revents = 0;
			break;
	}
	ioft->data = 0;
}