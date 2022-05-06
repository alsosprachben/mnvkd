#include "vk_poll.h"
#include "vk_state.h"
#include "debug.h"

void io_future_init(struct io_future *ioft, struct that *blocked_vk) {
	ioft->heap = blocked_vk->hd_ptr;
	ioft->blocked_vk = blocked_vk;
	switch (blocked_vk->waiting_socket_ptr->block.op) {
		case VK_OP_READ:
			ioft->event.fd = VK_PIPE_GET_FD(blocked_vk->waiting_socket_ptr->rx_fd);
			ioft->event.events = POLLIN;
			ioft->event.revents = 0;
			break;
		case VK_OP_WRITE:
		case VK_OP_FLUSH:
			ioft->event.fd = VK_PIPE_GET_FD(blocked_vk->waiting_socket_ptr->tx_fd);
			ioft->event.events = POLLOUT;
			ioft->event.revents = 0;
			break;
	}
	ioft->data = 0;
}