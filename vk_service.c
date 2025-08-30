/* Copyright 2022 BCW. All Rights Reserved. */
#include "vk_service.h"
#include "vk_fd_table.h"
#include "vk_kern.h"
#include "vk_proc.h"
#include "vk_service_s.h"
#include "vk_thread.h"
#include "vk_wrapguard.h"

void vk_service_listener(struct vk_thread* that)
{
	int rc = 0;
	struct {
		struct vk_service service; /* via vk_copy_arg */
		struct vk_server* server_ptr;
		int accepted_fd;
		struct vk_accepted* accepted_ptr;
		struct vk_thread* accepted_vk;
	}* self;
	vk_begin();

	self->server_ptr = &self->service.server;
	self->accepted_ptr = &self->service.accepted;

	/* binds a server socket to our read pipe, replacing what was there before */
	rc = vk_server_listen(self->server_ptr);
	if (rc == -1) {
		vk_perror("vk_server_listen");
		vk_error();
	}

	for (;;) {
		if (vk_kern_get_shutdown_requested(self->server_ptr->kern_ptr)) {
			break;
		}
		vk_accept(self->accepted_fd, self->accepted_ptr);
		vk_dbgf("vk_accept() from client %s:%s as FD %i\n", vk_accepted_get_address_str(self->accepted_ptr),
			vk_accepted_get_port_str(self->accepted_ptr), self->accepted_fd);

		if (self->accepted_fd > vk_fd_table_get_size(vk_kern_get_fd_table(self->server_ptr->kern_ptr))) {
			vk_logf("FD %i is beyond FD table maximum %zu. Closing.\n", self->accepted_fd,
				vk_fd_table_get_size(vk_kern_get_fd_table(self->server_ptr->kern_ptr)));
			rc = close(self->accepted_fd);
			if (rc == -1 && errno == EINTR) {
				rc = close(self->accepted_fd);
				if (rc == -1) {
					vk_perror("close");
				}
			}
			continue;
		}

		vk_accepted_set_proc(self->accepted_ptr, vk_kern_alloc_proc(self->server_ptr->kern_ptr,
									    vk_server_get_count(self->server_ptr) > 0
										? vk_server_get_pool(self->server_ptr)
										: NULL));
		if (vk_accepted_get_proc(self->accepted_ptr) == NULL) {
			vk_error();
		}

		if (vk_server_get_count(self->server_ptr) > 0) {
			/* Use a heap pool */
			rc = vk_proc_alloc_from_pool(vk_accepted_get_proc(self->accepted_ptr),
						     vk_server_get_pool(self->server_ptr));
			if (rc == -1) {
				if (errno == ENOMEM) {
					/* If beyond count, do not use a heap pool */
					rc = VK_PROC_INIT_PRIVATE(
					    vk_accepted_get_proc(self->accepted_ptr),
					    vk_pagesize() * vk_server_get_page_count(self->server_ptr), 1, 0);
					if (rc == -1) {
						vk_error();
					}
				} else {
					vk_error();
				}
			}
		} else {
			/* If no explicit count, do not use a heap pool */
			rc = VK_PROC_INIT_PRIVATE(vk_accepted_get_proc(self->accepted_ptr),
						  vk_pagesize() * vk_server_get_page_count(self->server_ptr), 1, 0);
			if (rc == -1) {
				vk_error();
			}
		}
		vk_proc_set_privileged(vk_accepted_get_proc(self->accepted_ptr),
				       vk_server_get_privileged(self->server_ptr));
		vk_proc_set_isolated(vk_accepted_get_proc(self->accepted_ptr),
				     vk_server_get_isolated(self->server_ptr));

		self->accepted_vk = vk_proc_alloc_thread(vk_accepted_get_proc(self->accepted_ptr));
		vk_accepted_set_vk(self->accepted_ptr, self->accepted_vk);
		if (vk_accepted_get_vk(self->accepted_ptr) == NULL) {
			vk_error();
		}

		VK_INIT(vk_accepted_get_vk(self->accepted_ptr),
			vk_proc_get_local(vk_accepted_get_proc(self->accepted_ptr)),
			vk_server_get_vk_func(self->server_ptr), self->accepted_fd, VK_FD_TYPE_SOCKET_STREAM,
			self->accepted_fd, VK_FD_TYPE_SOCKET_STREAM);

		rc = vk_copy_arg(vk_accepted_get_vk(self->accepted_ptr), &self->service, sizeof(self->service));
		if (rc == -1) {
			vk_error();
		}
		vk_play(vk_accepted_get_vk(self->accepted_ptr));
		vk_kern_flush_proc_queues(self->server_ptr->kern_ptr, vk_accepted_get_proc(self->accepted_ptr));
		vk_heap_exit(vk_proc_get_heap(vk_accepted_get_proc(self->accepted_ptr)));
	}

	vk_finally();
	if (errno) {
		vk_perror("listener");
	}

	vk_end();
}
