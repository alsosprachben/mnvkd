#include <stdio.h>

#include "vk_main_local.h"
#include "vk_future_s.h"

void responder(struct vk_thread *that);

void requestor(struct vk_thread *that)
{
	struct {
		struct vk_future request_ft;
		struct vk_future* response_ft_ptr;
		struct vk_thread* response_vk_ptr;
		int request_i;
		int* response_i_ptr;
	}* self;
	vk_begin();

	self->request_i = 5;
	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());
	vk_child(self->response_vk_ptr, responder);

	dprintf(1, "Request at requestor: %i\n", self->request_i);

	vk_request(self->response_vk_ptr, &self->request_ft, &self->request_i, self->response_ft_ptr, self->response_i_ptr);

	dprintf(1, "Response at requestor: %i\n", *self->response_i_ptr);

	vk_end();
}

void responder(struct vk_thread *that)
{
	struct {
		struct vk_future* parent_ft_ptr;
		struct vk_future child_ft;
		int* request_i_ptr;
		int response_i;
	}* self;
	vk_begin();

	vk_listen(self->parent_ft_ptr);

	self->request_i_ptr = vk_future_get(self->parent_ft_ptr);

	dprintf(1, "Request at responder: %i\n", *self->request_i_ptr);

	self->response_i = (*self->request_i_ptr) + 2;

	dprintf(1, "Response at responder: %i\n", self->response_i);

	vk_future_bind(&self->child_ft, self->parent_ft_ptr->vk);
	vk_future_resolve(&self->child_ft, &self->response_i);
	vk_respond(&self->child_ft);

	vk_end();
}

int main() {
	return vk_local_main_init(requestor, NULL, 0, 34);
}