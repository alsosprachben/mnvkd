#include "vk_io_future.h"
#include "vk_io_future_s.h"
#include "vk_socket.h"

struct vk_socket *vk_io_future_get_socket(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->socket_ptr;
}
void vk_io_future_set_socket(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr) {
	ioft_ptr->socket_ptr = socket_ptr;
}

struct pollfd vk_io_future_get_event(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->event;
}
void vk_io_future_set_event(struct vk_io_future *ioft_ptr, struct pollfd event) {
	ioft_ptr->event = event;
}

intptr_t vk_io_future_get_data(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->data;
}
void vk_io_future_set_data(struct vk_io_future *ioft_ptr, intptr_t data) {
	ioft_ptr->data = data;
}
unsigned int vk_io_future_get_fflags(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->fflags;
}
void vk_io_future_set_fflags(struct vk_io_future *ioft_ptr, unsigned int fflags) {
	ioft_ptr->fflags = fflags;
}

int vk_io_future_get_closed(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->closed;
}
void vk_io_future_set_closed(struct vk_io_future *ioft_ptr, int closed) {
	ioft_ptr->closed = closed;
}

void vk_io_future_init(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr) {
	ioft_ptr->socket_ptr = socket_ptr;
	ioft_ptr->event.fd      = vk_socket_get_blocked_fd(socket_ptr);
	ioft_ptr->event.events  = vk_socket_get_blocked_events(socket_ptr);
	ioft_ptr->event.revents = 0;
	ioft_ptr->data = 0;
	ioft_ptr->fflags = 0;
	ioft_ptr->closed = 0;
}
