#include "vk_service.h"
#include "vk_thread_exec.h"
#include "vk_thread_io.h"

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

struct join_message {
        int close;
        size_t len;
        char text[64];
};

static void responder(struct vk_thread *that)
{
        struct {
                struct join_message msg;
                ssize_t rc;
        } *self;

        vk_begin();

        while (1) {
                vk_read(self->rc, (char *)&self->msg, sizeof(self->msg));
                vk_logf("responder read rc=%zd close=%d len=%zu\n", self->rc, self->msg.close, self->msg.len);
                if (self->rc == 0 || self->msg.close) {
                        break;
                }
                vk_write_literal("responder: ");
                vk_write(self->msg.text, self->msg.len);
                vk_write_literal("\n");
                vk_flush();
                vk_log("responder flushed message\n");
        }

        vk_flush();
        vk_log("responder closing tx\n");
        vk_tx_close();
        vk_end();
}

static void responder_service(struct vk_thread *that)
{
        struct {
                struct join_message msg;
                struct vk_thread *responder_ptr;
                char line[64];
                ssize_t rc;
        } *self;

        vk_begin();

        vk_calloc_size(self->responder_ptr, 1, vk_alloc_size());
        vk_responder(self->responder_ptr, responder);
        vk_play(self->responder_ptr);

        vk_log("service started responder\n");

        while (1) {
                vk_readline(self->rc, self->line, sizeof(self->line) - 1);
                vk_logf("service readline rc=%zd\n", self->rc);
                if (self->rc <= 0) {
                        break;
                }
                if (self->line[self->rc - 1] == '\n') {
                        self->line[self->rc - 1] = '\0';
                } else {
                        self->line[self->rc] = '\0';
                }
                memset(&self->msg, 0, sizeof(self->msg));
                strncpy(self->msg.text, self->line, sizeof(self->msg.text) - 1);
                self->msg.len = strnlen(self->msg.text, sizeof(self->msg.text));
                vk_write((char *)&self->msg, sizeof(self->msg));
                vk_flush();
                vk_log("service forwarded message\n");
        }

	memset(&self->msg, 0, sizeof(self->msg));
	self->msg.close = 1;
	vk_write((char *)&self->msg, sizeof(self->msg));
	vk_log("service sent close message\n");
	vk_flush();

	vk_join(self->responder_ptr);
	vk_tx_close();
	vk_free();
	vk_log("service closing connection\n");

	vk_end();
}

int main(int argc, char *argv[])
{
        int rc;
        struct vk_server *server_ptr;
        struct vk_pool *pool_ptr;
        struct sockaddr_in address;

        server_ptr = calloc(1, vk_server_alloc_size());
        pool_ptr = calloc(1, vk_pool_alloc_size());

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(9089);

        vk_server_set_pool(server_ptr, pool_ptr);
        vk_server_set_socket(server_ptr, PF_INET, SOCK_STREAM, 0);
        vk_server_set_address(server_ptr, (struct sockaddr *)&address, sizeof(address));
        vk_server_set_backlog(server_ptr, 16);
        vk_server_set_reuseport(server_ptr, 1);
        vk_server_set_vk_func(server_ptr, responder_service);
        vk_server_set_count(server_ptr, 0);
        vk_server_set_privileged(server_ptr, 1);
        vk_server_set_isolated(server_ptr, 0);
        vk_server_set_page_count(server_ptr, 25);
        vk_server_set_msg(server_ptr, NULL);

        rc = vk_server_init(server_ptr);
        if (rc == -1) {
                return 1;
        }

        return 0;
}
