#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "vk_kern.h"
#include "vk_proc.h"
#include "vk_service.h"
#include "vk_service_s.h"
#include "vk_thread.h"

void vk_service_listener(struct vk_thread *that) {
	int rc = 0;
	struct {
		struct vk_service service; /* via vk_copy_arg */
		struct vk_server *server_ptr;
		int accepted_fd;
		struct vk_accepted *accepted_ptr;
	} *self;
	vk_begin();

	self->server_ptr = &self->service.server;
	self->accepted_ptr = &self->service.accepted;

	vk_server_listen(self->server_ptr);

	for (;;) {
		vk_accept(self->accepted_fd, self->accepted_ptr);
		vk_dbg("vk_accept() from client %s:%s\n", vk_accepted_get_address_str(self->accepted_ptr), vk_accepted_get_port_str(self->accepted_ptr));

		vk_accepted_set_proc(self->accepted_ptr, vk_kern_alloc_proc(vk_proc_get_kern(vk_get_proc(that))));
		if (vk_accepted_get_proc(self->accepted_ptr) == NULL) {
			vk_error();
		}
		rc = VK_PROC_INIT_PRIVATE(vk_accepted_get_proc(self->accepted_ptr), 4096 * 25);
		if (rc == -1) {
			vk_error();
		}

		

		vk_accepted_set_vk(self->accepted_ptr, vk_proc_alloc_that(vk_accepted_get_proc(self->accepted_ptr)));
		if (vk_accepted_get_vk(self->accepted_ptr) == NULL) {
			vk_error();
		}

		VK_INIT(vk_accepted_get_vk(self->accepted_ptr), vk_accepted_get_proc(self->accepted_ptr), vk_server_get_vk_func(self->server_ptr), self->accepted_fd, self->accepted_fd);
		
		vk_copy_arg(vk_accepted_get_vk(self->accepted_ptr), &self->service, sizeof (self->service));
		vk_play(vk_accepted_get_vk(self->accepted_ptr));
		vk_kern_flush_proc_queues(vk_proc_get_kern(vk_accepted_get_proc(self->accepted_ptr)), vk_accepted_get_proc(self->accepted_ptr));
	}

	vk_finally();
	if (errno) {
		vk_perror("listener");
	}

	vk_end();
}