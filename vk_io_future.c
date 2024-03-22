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

size_t vk_io_future_get_readable(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->readable;
}
void vk_io_future_set_readable(struct vk_io_future *ioft_ptr, size_t readable) {
	ioft_ptr->readable = readable;
}
size_t vk_io_future_get_writable(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->writable;
}
void vk_io_future_set_writable(struct vk_io_future *ioft_ptr, size_t writable) {
	ioft_ptr->writable = writable;
}

int vk_io_future_get_closed(struct vk_io_future *ioft_ptr) {
	return ioft_ptr->closed;
}
void vk_io_future_set_closed(struct vk_io_future *ioft_ptr, int closed) {
	ioft_ptr->closed = closed;
}

void vk_io_future_init(struct vk_io_future *ioft_ptr, struct vk_socket *socket_ptr) {
	ioft_ptr->socket_ptr = socket_ptr;
	ioft_ptr->event.fd      = -1;
	ioft_ptr->event.events  = 0;
	ioft_ptr->event.revents = 0;
	ioft_ptr->readable = 0;
	ioft_ptr->writable = 0;
	ioft_ptr->closed = vk_socket_get_blocked_closed(socket_ptr);
}
