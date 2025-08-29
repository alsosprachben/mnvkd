#include "vk_thread.h"
#include "vk_service_s.h"
#include <string.h>
#include <errno.h>

void vk_redis_hello(struct vk_thread* that)
{
        int rc = 0;

        struct {
                struct vk_service service; /* via vk_copy_arg() */
                char line[128];
        }* self;

        vk_begin();

        /* Expect a minimal RESP PING: *1\r\n$4\r\nPING\r\n */
        vk_readline(rc, self->line, sizeof(self->line) - 1);
        if (rc <= 0 || vk_eof()) {
                vk_raise(EPIPE);
        }

        vk_readline(rc, self->line, sizeof(self->line) - 1);
        if (rc <= 0 || vk_eof()) {
                vk_raise(EPIPE);
        }

        vk_readline(rc, self->line, sizeof(self->line) - 1);
        if (rc >= 4 && strncmp(self->line, "PING", 4) == 0) {
                const char resp[] = "+Hello World\r\n";
                vk_write(resp, sizeof(resp) - 1);
                vk_flush();
        }

        vk_finally();
        if (errno) {
                vk_perror("vk_redis_hello");
        }

        vk_end();
}
