#include "vk_redis.h"
#include "vk_service_s.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

void redis_response(struct vk_thread* that)
{
        ssize_t rc = 0;

        struct {
                struct vk_service* service_ptr; /* via redis_request via vk_send() */
                struct redis_query query;
        }* self;

        vk_begin();

        vk_recv(self->service_ptr);

        do {
                vk_read(rc, (char*)&self->query, sizeof(self->query));
                if (rc == 0) {
                        break;
                }
                if (rc != sizeof(self->query)) {
                        vk_raise(EPIPE);
                }

                if (self->query.argc > 0 && strcasecmp(self->query.argv[0], "PING") == 0) {
                        vk_write_literal("+PONG\r\n");
                        vk_flush();
                } else {
                        vk_write_literal("-ERR unknown command\r\n");
                        vk_flush();
                }
        } while (!vk_nodata());

        vk_finally();
        if (errno) {
                vk_perror("redis_response");
        }
        vk_free();
        vk_end();
}

void redis_request(struct vk_thread* that)
{
        ssize_t rc = 0;

        struct {
                struct vk_service service; /* via vk_copy_arg() */
                struct redis_query query;
                struct vk_thread* response_vk_ptr;
                struct vk_future query_ft;
                char line[128];
                int i;
                int len;
        }* self;

        vk_begin();

        vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());
        vk_responder(self->response_vk_ptr, redis_response);
        vk_send(self->response_vk_ptr, &self->query_ft, &self->service);

        do {
                vk_readline(rc, self->line, sizeof(self->line) - 1);
                if (rc <= 0) {
                        vk_raise(EPIPE);
                }
                self->line[rc] = '\0';
                if (self->line[0] != '*') {
                        vk_raise(EINVAL);
                }
                self->query.argc = atoi(self->line + 1);
                if (self->query.argc <= 0 || self->query.argc > REDIS_MAX_ARGS) {
                        vk_raise(EINVAL);
                }
                for (self->i = 0; self->i < self->query.argc; ++self->i) {
                        vk_readline(rc, self->line, sizeof(self->line) - 1);
                        if (rc <= 0 || self->line[0] != '$') {
                                vk_raise(EPIPE);
                        }
                        self->len = atoi(self->line + 1);
                        if (self->len >= REDIS_MAX_BULK) {
                                self->len = REDIS_MAX_BULK - 1;
                        }
                        vk_read(rc, self->query.argv[self->i], self->len);
                        if (rc != self->len) {
                                vk_raise(EPIPE);
                        }
                        self->query.argv[self->i][self->len] = '\0';
                        vk_readline(rc, self->line, sizeof(self->line) - 1);
                        if (rc <= 0) {
                                vk_raise(EPIPE);
                        }
                }
                vk_write((char*)&self->query, sizeof(self->query));
                vk_flush();
        } while (!vk_nodata());

        vk_finally();
        vk_call(self->response_vk_ptr);
        if (errno) {
                vk_sigerror();
                vk_perror("redis_request");
        }
        vk_end();
}
