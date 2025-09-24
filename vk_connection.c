#include "vk_connection.h"
#include "vk_connection_s.h"
#include "vk_client.h"
#include "vk_accepted.h"
#include "vk_proc.h"
#include "vk_thread.h"
#include "vk_wrapguard.h"
#include <errno.h>
#include <unistd.h>

/* The receiver coroutine is supplied by the client (protocol-specific). */

void vk_connection_client(struct vk_thread* that)
{
    int rc = 0;
    (void)rc;
    struct {
        struct vk_connection connection; /* via vk_copy_arg */
        int fd;
        struct vk_thread* sender_vk_ptr;
        struct vk_thread* receiver_vk_ptr;
    } *self;
    vk_begin();

    vk_connect(self->fd, &self->connection.accepted, vk_client_get_address(&self->connection.client), vk_client_get_port(&self->connection.client));
    if (self->fd == -1) {
        vk_perror("vk_connect");
        vk_error();
    }

    vk_dbgf("client: connected fd=%d to %s:%s",
            self->fd,
            vk_accepted_get_address_str(&self->connection.accepted),
            vk_accepted_get_port_str(&self->connection.accepted));

    /* Child sender: stdin -> socket */
    vk_calloc_size(self->sender_vk_ptr, 1, vk_alloc_size());
    VK_INIT(self->sender_vk_ptr, vk_get_proc_local(that),
            vk_client_get_vk_req_func(&self->connection.client), 0,
            VK_FD_TYPE_SOCKET_STREAM, self->fd, VK_FD_TYPE_SOCKET_STREAM);

    /* Child receiver: socket -> stdout */
    vk_calloc_size(self->receiver_vk_ptr, 1, vk_alloc_size());
    VK_INIT(self->receiver_vk_ptr, vk_get_proc_local(that),
            vk_client_get_vk_resp_func(&self->connection.client), self->fd,
            VK_FD_TYPE_SOCKET_STREAM, 1, VK_FD_TYPE_SOCKET_STREAM);

    /* Run both children */
    vk_play(self->receiver_vk_ptr);
    vk_play(self->sender_vk_ptr);
    vk_dbgf("client: started children sender=%p receiver=%p", (void*)self->sender_vk_ptr, (void*)self->receiver_vk_ptr);

    /* Cooperatively wait until both children complete */
    while (!(vk_is_completed(self->sender_vk_ptr) && vk_is_completed(self->receiver_vk_ptr))) {
        vk_pause();
    }

    vk_finally();
    vk_end();
}
