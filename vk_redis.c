#include "vk_redis.h"
#include "vk_future_s.h" /* for `struct vk_future` */
#include "vk_service_s.h"
#include "vk_kern.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>

void redis_response(struct vk_thread* that)
{
        int rc;
        struct {
                struct vk_service* service_ptr; /* via redis_request via vk_send() */
                struct redis_query query;
                sqlite3* db;
                sqlite3_stmt* stmt;
                int reply_len;
                char reply[REDIS_MAX_BULK + 32];
                ssize_t rc;
        }* self;

        vk_begin();

        if (sqlite3_open("vk_redis.db", &self->db) != SQLITE_OK) {
                vk_raise(EFAULT);
        }
        sqlite3_exec(self->db,
                      "CREATE TABLE IF NOT EXISTS kv(key TEXT PRIMARY KEY, value TEXT);",
                      NULL, NULL, NULL);
        self->stmt = NULL;
        self->reply_len = 0;

        vk_recv(self->service_ptr);
        vk_dbgf("redis_response: received service_ptr=%p kern_ptr=%p",
                (void*)self->service_ptr, (void*)self->service_ptr->server.kern_ptr);

        do {
		/*
		 * Block for the next query so we don't race pipelined commands.
		 * Using vk_nodata() here proved lossy once we removed the hup/readhup
		 * handshake, so rely solely on blocking reads and explicit CLOSE flags.
		 */
		vk_read(self->rc, (char*)&self->query, sizeof(self->query));
		if (self->rc == 0) {
			break;
		}
		if (self->rc != sizeof(self->query)) {
			vk_raise(EPIPE);
		}

                vk_dbgf("redis_response: query argc=%d argv0=%s shutdown=%d close=%d",
                        self->query.argc,
                        self->query.argc > 0 ? self->query.argv[0] : "",
                        self->query.shutdown, self->query.close);

                if (self->query.shutdown) {
                        /* Request kernel shutdown, then stop writing. */
                        vk_dbgf("redis_response: SHUTDOWN setting kernel flag kern_ptr=%p",
                                (void*)self->service_ptr->server.kern_ptr);
                        if (self->service_ptr && self->service_ptr->server.kern_ptr) {
                                vk_kern_set_shutdown_requested(self->service_ptr->server.kern_ptr);
                                vk_tx_close();
                                vk_dbg("redis_response: tx closed after SHUTDOWN");
                        }
                        break;
                } else if (self->query.close) {
                        vk_dbg("redis_response: CLOSE requested");
                        vk_write_literal("+OK\r\n");
                        vk_flush(); /* explicit flush for pipeline chaining */
                        /* Close write-side to signal EOF to client only for networked server */
                        if (self->service_ptr && self->service_ptr->server.kern_ptr) {
                                vk_tx_close();
                                vk_dbg("redis_response: tx closed after CLOSE");
                        }
                        break;
                } else if (self->query.argc > 0 && strcasecmp(self->query.argv[0], "PING") == 0) {
                        vk_dbg("redis_response: PING -> PONG");
                        vk_write_literal("+PONG\r\n");
                        vk_flush(); /* explicit flush for pipeline chaining */
                } else if (self->query.argc >= 3 &&
                           strcasecmp(self->query.argv[0], "SET") == 0) {
                        self->stmt = NULL;
                        if (sqlite3_prepare_v2(
                                    self->db,
                                    "INSERT INTO kv(key,value) VALUES(?1,?2) "
                                    "ON CONFLICT(key) DO UPDATE SET value=excluded.value;",
                                    -1, &self->stmt, NULL) == SQLITE_OK) {
                                sqlite3_bind_text(self->stmt, 1, self->query.argv[1], -1, SQLITE_STATIC);
                                sqlite3_bind_text(self->stmt, 2, self->query.argv[2], -1, SQLITE_STATIC);
                                if (sqlite3_step(self->stmt) == SQLITE_DONE) {
                                        sqlite3_finalize(self->stmt);
                                        self->stmt = NULL;
                                        vk_write_literal("+OK\r\n");
                                } else {
                                        sqlite3_finalize(self->stmt);
                                        self->stmt = NULL;
                                        vk_write_literal("-ERR sqlite\r\n");
                                }
                        } else {
                                vk_write_literal("-ERR sqlite\r\n");
                        }
                        if (self->stmt) {
                                sqlite3_finalize(self->stmt);
                                self->stmt = NULL;
                        }
                        vk_flush(); /* explicit flush for pipeline chaining */
                } else if (self->query.argc >= 2 &&
                           strcasecmp(self->query.argv[0], "GET") == 0) {
                        self->stmt = NULL;
                        if (sqlite3_prepare_v2(self->db,
                                               "SELECT value FROM kv WHERE key=?1;",
                                               -1, &self->stmt, NULL) == SQLITE_OK) {
                                sqlite3_bind_text(self->stmt, 1, self->query.argv[1], -1, SQLITE_STATIC);
                                if (sqlite3_step(self->stmt) == SQLITE_ROW) {
                                        const unsigned char* value = sqlite3_column_text(self->stmt, 0);
                                        if (value) {
                                                self->reply_len = strlen((const char*)value);
                                                if (self->reply_len >= REDIS_MAX_BULK) {
                                                        self->reply_len = REDIS_MAX_BULK - 1;
                                                }
                                                int prefix = snprintf(self->reply, sizeof(self->reply), "$%d\r\n", self->reply_len);
                                                if (prefix < (int)sizeof(self->reply)) {
                                                        memcpy(self->reply + prefix, value, self->reply_len);
                                                        self->reply[prefix + self->reply_len] = '\r';
                                                        self->reply[prefix + self->reply_len + 1] = '\n';
                                                        sqlite3_finalize(self->stmt);
                                                        self->stmt = NULL;
                                                        vk_write(self->reply, prefix + self->reply_len + 2);
                                                } else {
                                                        sqlite3_finalize(self->stmt);
                                                        self->stmt = NULL;
                                                        vk_write_literal("$-1\r\n");
                                                }
                                        } else {
                                                sqlite3_finalize(self->stmt);
                                                self->stmt = NULL;
                                                vk_write_literal("$-1\r\n");
                                        }
                                } else {
                                        sqlite3_finalize(self->stmt);
                                        self->stmt = NULL;
                                        vk_write_literal("$-1\r\n");
                                }
                        } else {
                                vk_write_literal("-ERR sqlite\r\n");
                        }
                        if (self->stmt) {
                                sqlite3_finalize(self->stmt);
                                self->stmt = NULL;
                        }
                        vk_flush(); /* explicit flush for pipeline chaining */
                } else {
                        vk_write_literal("-ERR unknown command\r\n");
                        vk_flush(); /* explicit flush for pipeline chaining */
                }
	} while (!self->query.close);

	vk_dbg("closing write-side");
	/* vk_tx_shutdown(); */
	vk_tx_close();
	vk_dbg("closed");

        errno = 0;
        vk_finally();
        if (self) {
                if (self->stmt) {
                        sqlite3_finalize(self->stmt);
                        self->stmt = NULL;
                }
                if (self->db) {
                        sqlite3_close(self->db);
                        self->db = NULL;
                }
        }
        if (errno) {
                vk_perror("redis_response");
                vk_sigerror();

                vk_flush();
                vk_tx_close();
        }
        /* Do not vk_free() the responder's self here; it is popped by
         * vk_deinit() when the coroutine ends. The parent frees the
         * responder thread object after vk_call() returns.
         */
        vk_end();
}

void redis_request(struct vk_thread* that)
{
        int rc;
	struct {
		struct vk_service service; /* via vk_copy_arg() */
		struct redis_query query;
		struct vk_thread* response_vk_ptr;
		struct vk_future query_ft;
		char line[128];
		int i;
		int len;
		ssize_t rc;
	}* self;

	vk_begin();

	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());
	vk_responder(self->response_vk_ptr, redis_response);
	vk_send(self->response_vk_ptr, &self->query_ft, &self->service);
        do {
                self->query.close = 0;
                vk_readline(self->rc, self->line, sizeof(self->line) - 1);
                if (self->rc < 0) {
                        vk_raise(EPIPE);
                }
                if (self->rc == 0) {
                        break;
                }
                self->line[self->rc] = '\0';
                if (self->line[0] != '*') {
                        vk_raise(EINVAL);
                }
                self->query.argc = atoi(self->line + 1);
                if (self->query.argc <= 0 || self->query.argc > REDIS_MAX_ARGS) {
                        vk_raise(EINVAL);
                }
                for (self->i = 0; self->i < self->query.argc; ++self->i) {
                        vk_readline(self->rc, self->line, sizeof(self->line) - 1);
                        if (self->rc <= 0 || self->line[0] != '$') {
                                vk_raise(EPIPE);
                        }
                        self->len = atoi(self->line + 1);
                        if (self->len >= REDIS_MAX_BULK) {
                                self->len = REDIS_MAX_BULK - 1;
                        }
                        vk_read(self->rc, self->query.argv[self->i], self->len);
                        if (self->rc != self->len) {
                                vk_raise(EPIPE);
                        }
                        self->query.argv[self->i][self->len] = '\0';
                        vk_readline(self->rc, self->line, sizeof(self->line) - 1);
                        if (self->rc <= 0) {
                                vk_raise(EPIPE);
                        }
                }
                self->query.shutdown = 0;
                self->query.close = 0;
                if (self->query.argc > 0) {
                        if (strcasecmp(self->query.argv[0], "SHUTDOWN") == 0) {
                                self->query.shutdown = 1;
                                self->query.close = 1;
                                /* Let responder handle shutdown and close TX to ensure EOF reaches client. */
                        } else if (strcasecmp(self->query.argv[0], "QUIT") == 0) {
                                self->query.close = 1;
                        }
                }
                vk_dbgf("redis_request: enqueue query argc=%d argv0=%s shutdown=%d close=%d",
                        self->query.argc,
                        self->query.argc > 0 ? self->query.argv[0] : "",
                        self->query.shutdown, self->query.close);
                vk_write((char*)&self->query, sizeof(self->query));
                vk_flush();
        } while (!vk_nodata() && !self->query.close);


        vk_dbg("redis_request: loop end; calling responder");
	/*
	vk_dbg("closing read-side");
	vk_rx_shutdown();
	vk_dbg("closed");
	*/

	vk_flush();

	errno = 0;
	vk_finally();
	vk_join(self->response_vk_ptr);
	int saved_errno = errno;
	vk_dbg("closing responder pipe");
	vk_tx_close();
	if (saved_errno != 0) {
		errno = saved_errno;
	} while (!self->query.close);
	/* Now that the responder has completed, free its thread object that was
	 * allocated in this coroutine before spawning it.
	 */
	vk_free();
	if (errno != 0) {
		vk_dbgf("request errno=%d\n", errno);
		vk_sigerror();
		if (errno == EPIPE) {
			vk_dbg_perror("request_error");
		} else {
			vk_perror("request_error");
		}
	}
	else {
		vk_dbg("request completed without errno");
	}

	vk_dbg("end of request handler");
	vk_end();
}
void redis_client_request(struct vk_thread* that)
{
	struct {
		struct redis_query query;
		char line[256];
		char* tok;
		char* saveptr;
		int len;
		int i;
		ssize_t rc;
	}* self;

	vk_begin();
	vk_log("redis_client_request: start\n");

	do {
		self->query.argc = 0;
		vk_readline(self->rc, self->line, sizeof(self->line) - 1);
		vk_logf("redis_client_request: vk_readline rc=%zd nodata=%d eof=%d\n", self->rc, vk_nodata(),
		        vk_socket_eof(vk_get_socket(that)));
		if (self->rc <= 0) {
			vk_log("redis_client_request: no input; exiting loop\n");
			break;
		}
		self->line[self->rc] = '\0';

		self->saveptr = NULL;
		for (self->tok = strtok_r(self->line, " \r\n", &self->saveptr);
		     self->tok && self->query.argc < REDIS_MAX_ARGS;
		     self->tok = strtok_r(NULL, " \r\n", &self->saveptr)) {
			strncpy(self->query.argv[self->query.argc], self->tok, REDIS_MAX_BULK - 1);
			self->query.argv[self->query.argc][REDIS_MAX_BULK - 1] = '\0';
			self->query.argc++;
		}

                if (self->query.argc == 0) {
                        continue;
                }

                self->query.shutdown = 0;
                self->query.close = 0;
                if (strcasecmp(self->query.argv[0], "SHUTDOWN") == 0) {
                        self->query.shutdown = 1;
                        self->query.close = 1;
                } else if (strcasecmp(self->query.argv[0], "QUIT") == 0) {
                        self->query.close = 1;
                }

		self->rc = snprintf(self->line, sizeof(self->line), "*%d\r\n", self->query.argc);
		if (self->rc > 0 && self->rc < (ssize_t)sizeof(self->line)) {
		        vk_write(self->line, (size_t)self->rc);
		}
		for (self->i = 0; self->i < self->query.argc; ++self->i) {
		        self->len = (int)strlen(self->query.argv[self->i]);
		        self->rc = snprintf(self->line, sizeof(self->line), "$%d\r\n", self->len);
		        if (self->rc > 0 && self->rc < (ssize_t)sizeof(self->line)) {
		                vk_write(self->line, (size_t)self->rc);
		        }
		        vk_write(self->query.argv[self->i], self->len);
		        vk_write_literal("\r\n");
		}
		vk_flush();
		vk_logf("redis_client_request: loop tail nodata=%d close=%d\n", vk_nodata(), self->query.close);
	} while (!vk_nodata() && !self->query.close);

	vk_log("redis_client_request: closing tx and exiting\n");
	vk_tx_close();
	vk_end();
}

void redis_client(struct vk_thread* that) { redis_client_request(that); }

/* receiver: socket -> stdout; auto-flush occurs when reads block */
void redis_client_response(struct vk_thread* that)
{
    struct {
        char line[256];
        int len;
        int bulk_len;
        char bulk[REDIS_MAX_BULK];
        ssize_t rc;
    } *self;
    vk_begin();
    vk_dbg("redis_client_response: start socket->stdout");
    for (;;) {
        /* Read one RESP header line */
        vk_readline(self->rc, self->line, sizeof(self->line) - 1);
        vk_logf("redis_client_response: read rc=%zd\n", self->rc);
        if (self->rc < 0) {
            vk_raise(EPIPE);
        }
        if (self->rc == 0) {
            break; /* EOF */
        }
        self->line[self->rc] = '\0';

        {
            char t = self->line[0];
            vk_logf("redis_client_response: type %c line=%s\n", t, self->line);
            if (t == '+') { /* Simple String */
                /* Strip CRLF and '+' */
                self->len = (int)self->rc - 3; /* minus '\r\n' and type */
                if (self->len < 0) self->len = 0;
                if (self->len > 0) {
                    vk_write(self->line + 1, (size_t)self->len);
                }
                vk_write_literal("\n");
            } else if (t == '-') { /* Error */
                /* Echo error content without duplicating the error code prefix. */
                self->len = (int)self->rc - 3; /* minus '\r\n' and type */
                if (self->len < 0) self->len = 0;
                if (self->len > 0) {
                    vk_write(self->line + 1, (size_t)self->len);
                }
                vk_write_literal("\n");
            } else if (t == ':') { /* Integer */
                self->len = (int)self->rc - 3;
                if (self->len < 0) self->len = 0;
                if (self->len > 0) {
                    vk_write(self->line + 1, (size_t)self->len);
                }
                vk_write_literal("\n");
            } else if (t == '$') { /* Bulk String */
                self->bulk_len = atoi(self->line + 1);
                if (self->bulk_len < 0) {
                    vk_write_literal("(nil)\n");
                } else {
                    if (self->bulk_len >= (int)sizeof(self->bulk)) {
                        /* Clamp to buffer size; read only up to capacity */
                        self->bulk_len = (int)sizeof(self->bulk) - 1;
                    }
                    /* Read bulk payload */
                    vk_read(self->rc, self->bulk, (size_t)self->bulk_len);
                    if (self->rc != self->bulk_len) {
                        vk_raise(EPIPE);
                    }
                    self->bulk[self->bulk_len] = '\0';
                    /* Consume trailing CRLF */
                    vk_readline(self->rc, self->line, sizeof(self->line) - 1);
                    if (self->rc <= 0) {
                        vk_raise(EPIPE);
                    }
                    vk_write(self->bulk, (size_t)self->bulk_len);
                    vk_write_literal("\n");
                }
            } else if (t == '*') { /* Array: print header */
                int count = atoi(self->line + 1);
                char hdr[64];
                int n = snprintf(hdr, sizeof(hdr), "(array) %d\n", count);
                if (n > 0 && n < (int)sizeof(hdr)) {
                    vk_write(hdr, (size_t)n);
                }
            } else { /* Unknown: echo line without CRLF */
                self->len = (int)self->rc - 2;
                if (self->len > 0) {
                    vk_write(self->line, (size_t)self->len);
                }
                vk_write_literal("\n");
            }
        }
        vk_flush();
    }
    /* Ensure any buffered output is flushed and stdout is closed for pipelines. */
    vk_flush();
    vk_tx_close();
    vk_end();
}
