#include "vk_main.h"
#include "vk_thread_exec.h"
#include "vk_thread_io.h"
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

        do {
                vk_read(self->rc, (char *)&self->msg, sizeof(self->msg));
                if (self->rc == 0 || self->msg.close) {
                        break;
                }
                vk_write_literal("responder: ");
                vk_write(self->msg.text, self->msg.len);
                vk_write_literal("\n");
                vk_flush();
        } while (1);

        vk_flush();
        vk_tx_close();
        vk_end();
}

static void parent(struct vk_thread *that)
{
        struct {
                struct vk_thread *responder_ptr;
                struct join_message msg;
                char line[64];
                ssize_t rc;
        } *self;

        vk_begin();

	vk_calloc_size(self->responder_ptr, 1, vk_alloc_size());
	vk_responder(self->responder_ptr, responder);
        vk_play(self->responder_ptr);

        while (1) {
                vk_readline(self->rc, self->line, sizeof(self->line) - 1);
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
        }

        memset(&self->msg, 0, sizeof(self->msg));
        self->msg.close = 1;
        self->msg.len = 0;
        vk_write((char *)&self->msg, sizeof(self->msg));
        vk_flush();

        vk_join(self->responder_ptr);
        vk_tx_close();
        vk_free();
        vk_end();
}

int main(void)
{
        return vk_main_init(parent, NULL, 0, 32, 1, 0);
}
