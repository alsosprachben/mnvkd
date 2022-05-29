#include "vk_poll_s.h"
#include "vk_socket.h"
#include "vk_state.h"
#include "debug.h"

void io_future_init(struct io_future *ioft, struct vk_socket *socket_ptr) {
	ioft->socket_ptr = socket_ptr;
	ioft->proc_ptr = vk_get_proc(vk_block_get_vk(vk_socket_get_block(socket_ptr)));
	switch (vk_block_get_op(vk_socket_get_block(socket_ptr))) {
		case VK_OP_READ:
			ioft->event.fd = vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr));
			ioft->event.events = POLLIN;
			ioft->event.revents = 0;
			break;
		case VK_OP_WRITE:
		case VK_OP_FLUSH:
			ioft->event.fd = vk_pipe_get_fd(vk_socket_get_tx_fd(socket_ptr));
			ioft->event.events = POLLOUT;
			ioft->event.revents = 0;
			break;
	}
	ioft->data = 0;
}