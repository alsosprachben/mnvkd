#include "vk_redis.h"
#include "vk_future_s.h" /* for `struct vk_future` */
#include "vk_service_s.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>

void redis_response(struct vk_thread* that)
{
	ssize_t rc = 0;

        struct {
                struct vk_service* service_ptr; /* via redis_request via vk_send() */
                struct redis_query query;
                sqlite3* db;
        }* self;

        vk_begin();

        if (sqlite3_open("vk_redis.db", &self->db) != SQLITE_OK) {
                vk_raise(EFAULT);
        }
        sqlite3_exec(self->db,
                      "CREATE TABLE IF NOT EXISTS kv(key TEXT PRIMARY KEY, value TEXT);",
                      NULL, NULL, NULL);

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
                } else if (self->query.argc >= 3 &&
                           strcasecmp(self->query.argv[0], "SET") == 0) {
                        sqlite3_stmt* stmt;
                        if (sqlite3_prepare_v2(
                                    self->db,
                                    "INSERT INTO kv(key,value) VALUES(?1,?2) "
                                    "ON CONFLICT(key) DO UPDATE SET value=excluded.value;",
                                    -1, &stmt, NULL) == SQLITE_OK) {
                                sqlite3_bind_text(stmt, 1, self->query.argv[1], -1, SQLITE_STATIC);
                                sqlite3_bind_text(stmt, 2, self->query.argv[2], -1, SQLITE_STATIC);
                                if (sqlite3_step(stmt) == SQLITE_DONE) {
                                        vk_write_literal("+OK\r\n");
                                        vk_flush();
                                } else {
                                        vk_write_literal("-ERR sqlite\r\n");
                                        vk_flush();
                                }
                                sqlite3_finalize(stmt);
                        } else {
                                vk_write_literal("-ERR sqlite\r\n");
                                vk_flush();
                        }
                } else if (self->query.argc >= 2 &&
                           strcasecmp(self->query.argv[0], "GET") == 0) {
                        sqlite3_stmt* stmt;
                        if (sqlite3_prepare_v2(self->db,
                                               "SELECT value FROM kv WHERE key=?1;",
                                               -1, &stmt, NULL) == SQLITE_OK) {
                                sqlite3_bind_text(stmt, 1, self->query.argv[1], -1, SQLITE_STATIC);
                                if (sqlite3_step(stmt) == SQLITE_ROW) {
                                        const unsigned char* value;
                                        int len;
                                        char reply[REDIS_MAX_BULK + 32];
                                        value = sqlite3_column_text(stmt, 0);
                                        if (!value) {
                                                vk_write_literal("$-1\r\n");
                                                vk_flush();
                                        } else {
                                                len = strlen((const char*)value);
                                                if (len >= REDIS_MAX_BULK) {
                                                        len = REDIS_MAX_BULK - 1;
                                                }
                                                int n = snprintf(reply, sizeof(reply), "$%d\r\n", len);
                                                if (n < (int)sizeof(reply)) {
                                                        memcpy(reply + n, value, len);
                                                        reply[n + len] = '\r';
                                                        reply[n + len + 1] = '\n';
                                                        vk_write(reply, n + len + 2);
                                                        vk_flush();
                                                } else {
                                                        vk_write_literal("$-1\r\n");
                                                        vk_flush();
                                                }
                                        }
                                } else {
                                        vk_write_literal("$-1\r\n");
                                        vk_flush();
                                }
                                sqlite3_finalize(stmt);
                        } else {
                                vk_write_literal("-ERR sqlite\r\n");
                                vk_flush();
                        }
                } else {
                        vk_write_literal("-ERR unknown command\r\n");
                        vk_flush();
                }
        } while (!vk_nodata());

        vk_flush();
        vk_tx_close();

        errno = 0;
        vk_finally();
        if (self && self->db) {
                sqlite3_close(self->db);
        }
        if (errno) {
                vk_perror("redis_response");
                vk_flush();
                vk_tx_close();
        }
        if (self) {
                vk_free();
        }
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
