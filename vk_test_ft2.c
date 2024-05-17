#include <stdio.h>

#include "vk_main_local.h"
#include "vk_future_s.h"

void responder(struct vk_thread *that);

void requestor(struct vk_thread *that)
{
	struct {
		struct vk_future request_ft;
		struct vk_thread* response_vk_ptr;
		int request_i;
		int* response_i_ptr;
	}* self;
	vk_begin();

	self->request_i = 5;
	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());
	vk_child(self->response_vk_ptr, responder);

	/*
	 * parent::vk_send -> child::vk_listen -> child::vk_send -> parent::vk_listen
	 */
	vk_logf("LOG Request at requestor: %i\n", self->request_i);

	vk_send(self->response_vk_ptr, &self->request_ft, &self->request_i);
	vk_pause();
	vk_recv(self->response_i_ptr);

	vk_logf("LOG Response at requestor: %i\n", *self->response_i_ptr);

	vk_free(); /* free self->response_vk_ptr */

	vk_end();
}

void responder(struct vk_thread *that)
{
	struct {
		struct vk_future* parent_ft_ptr;
		int* request_i_ptr;
		int response_i;
	}* self;
	vk_begin();

	/*
	 * parent::vk_send -> child::vk_listen -> child::vk_send -> parent::vk_listen
	 */
	vk_listen(self->parent_ft_ptr, self->request_i_ptr);

	vk_logf("LOG Request at responder: %i\n", *self->request_i_ptr);

	self->response_i = (*self->request_i_ptr) + 2;

	vk_logf("LOG Response at responder: %i\n", self->response_i);

	vk_send(self->parent_ft_ptr->vk, self->parent_ft_ptr, &self->response_i);

	vk_end();
}

int main() {
	return vk_local_main_init(requestor, NULL, 0, 34);
}