#include <stdlib.h> /* abort */

#include "vk_rfc.h"
#include "debug.h"

enum HTTP_METHOD {
	NO_METHOD,
	GET,
	HEAD,
	POST,
	PUT,
	DELETE,
	CONNECT,
	OPTIONS,
	TRACE,
	PATCH,
	PRI,
};
struct http_method {
	enum HTTP_METHOD method;
	char *repr;
};
static struct http_method methods[] = {
	{ GET,     "GET",     },
	{ HEAD,    "HEAD",    },
	{ POST,    "POST",    },
	{ PUT,     "PUT",     },
	{ DELETE,  "DELETE",  },
	{ CONNECT, "CONNECT", },
	{ OPTIONS, "OPTIONS", },
	{ TRACE,   "TRACE",   },
	{ PATCH,   "PATCH",   },
	{ PRI,     "PRI",     },
};
#define METHOD_COUNT 9

enum HTTP_VERSION {
	NO_VERSION,
	HTTP09,
	HTTP10,
	HTTP11,
	HTTP20,
};
struct http_version {
	enum HTTP_VERSION version;
	char *repr;
};
static struct http_version versions[] = {
	{ HTTP09, "HTTP/0.9", },
	{ HTTP10, "HTTP/1.0", },
	{ HTTP11, "HTTP/1.1", },
	{ HTTP20, "HTTP/2.0", },
};
#define VERSION_COUNT 4

enum HTTP_HEADER {
	TRANSFER_ENCODING,
	CONTENT_LENGTH,
	TRAILER,
	TE,
};
struct http_header {
	enum HTTP_HEADER http_header;
	char *repr;
};
static struct http_header http_headers[] = {
	{ TRANSFER_ENCODING, "Transfer-Encoding", },
	{ CONTENT_LENGTH, "Content-Length", },
	{ TRAILER, "Trailer", },
	{ TE, "TE", },
};
#define HTTP_HEADER_COUNT 2

enum HTTP_TRAILER {
	POST_CONTENT_LENGTH,
};
struct http_trailer {
	enum HTTP_TRAILER http_trailer;
	char *repr;
};
static struct http_trailer http_trailers[] = {
	{ POST_CONTENT_LENGTH, "Content-Length", },
};
#define HTTP_TRAILER_COUNT 1

struct header {
	char key[32];
	char val[96];
};

struct request {
	enum HTTP_METHOD method;
	char uri[1024];
	enum HTTP_VERSION version;
	struct header headers[15];
	int header_count;
	size_t content_length;
	int chunked;
};

struct chunk {
	char   buf[4096];
	size_t size;
	char   head[19]; // 16 (size_t hex) + 2 (\r\n) + 1 (\0)
};

#define vk_read_chunk(rc_arg, chunk_arg) do {                   \
	vk_read(rc_arg, (chunk_arg).buf, sizeof ((chunk_arg).buf)); \
	(chunk_arg).size = (size_t) (rc_arg);                       \
} while (0)

#define vk_write_chunk_proto(rc_arg, chunk_arg) do { \
	rc_arg = size_hex((chunk_arg).head, sizeof ((chunk_arg).head) - 1, (chunk_arg).size); \
	(chunk_arg).head[rc++] = '\r'; \
	(chunk_arg).head[rc++] = '\n'; \
	vk_write((chunk_arg).head, rc); \
	vk_write((chunk_arg).buf, (chunk_arg).size); \
} while (0)

#define vk_read_chunk_proto(rc_arg, chunk_arg) do { \
	vk_readrfcline(rc_arg, (chunk_arg).head, sizeof ((chunk_arg).head) - 1); \
	if (rc_arg == 0) { \
		break; \
	} \
	(chunk_arg).head[rc_arg] = '\0'; \
	if (rc_arg == 0) { \
		break; \
	} \
	(chunk_arg).size = hex_size((chunk_arg).head); \
	if ((chunk_arg).size == 0) { \
		rc_arg = (int) (chunk_arg).size; \
		break; \
	} \
	vk_read(rc_arg, (chunk_arg).buf, (chunk_arg).size); \
	if (rc_arg != (chunk_arg).size) { \
		vk_raise(EPIPE); \
	} \
} while (0)

void http11_response(struct that *that) {
	int rc = 0;

	struct {
		struct chunk    chunk;
		struct future   request_ft;
		struct request *request_ptr;
	} *self;

	vk_begin();

	for (;;) {
		/* get request */
		vk_listen(self->request_ft);
		self->request_ptr = future_get(self->request_ft);

		/* set request receipt */
		future_resolve(self->request_ft, 0);
		vk_respond(self->request_ft);

		/* write response header to socket */
		rc = snprintf(self->chunk.buf, sizeof (self->chunk.buf) - 1, "200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n");
		if (rc == -1) {
			vk_error();
		}
		vk_write(self->chunk.buf, strlen(self->chunk.buf));

		if (self->request_ptr->content_length > 0 || self->request_ptr->chunked) {
			/* write chunks */
			while (!vk_nodata()) {
				vk_read_chunk(rc, self->chunk);
				vk_dbg("chunk.size = %zu: %.*s\n", self->chunk.size, (int) self->chunk.size, self->chunk.buf);
				vk_write_chunk_proto(rc, self->chunk);
			}
			vk_clear();
		}

		vk_write("0\r\n\r\n", 5);
		vk_flush();
		
		vk_dbg("%s", "end of response\n");
	}

	vk_finally();
	if (errno != 0) {
		vk_perror("response error");
	}
	vk_end();
}

void http11_request(struct that *that) {
	int rc = 0;
	int i;

	struct {
		struct chunk chunk;
		ssize_t to_receive;
		int rc;
		char line[1024];
		char *tok;
		char *key;
		char *val;
		struct future return_ft;
		struct request request;
		intptr_t response;
		struct that response_vk;
	} *self;

	vk_begin();

	vk_responder(rc, &self->response_vk, http11_response, 4096 * 2);
	if (rc == -1) {
		vk_error();
	}

	vk_spawn(&self->response_vk, self->return_ft, NULL);

	do {
		/* request line */
		vk_readrfcline(rc, self->line, sizeof (self->line) - 1);
		if (rc == 0) {
			break;
		}
		self->line[rc] = '\0';
		if (rc == 0) {
			break;
		}

		/* request method */
		self->request.method = NO_METHOD;
		self->tok = self->line;
		self->val = strsep(&self->tok, " \t");
		for (i = 0; i < METHOD_COUNT; i++) {
			if (strcasecmp(self->val, methods[i].repr) == 0) {
				self->request.method = methods[i].method;
				break;
			}
		}

		/* request URI */
		self->val = strsep(&self->tok, " \t");
		copy_into(self->request.uri, self->val);

		/* request version */
		self->request.version = NO_VERSION;
		self->val = strsep(&self->tok, " \t");
		for (i = 0; i < VERSION_COUNT; i++) {
			if (strcasecmp(self->val, versions[i].repr) == 0) {
				self->request.version = versions[i].version;
				break;
			}
		}

		/* request headers */
		self->request.chunked = 0;
		self->request.content_length = 0;
		self->request.header_count = 0;
		for (;;) {
			vk_readrfcheader(rc, self->line, sizeof (self->line) - 1, &self->key, &self->val);
			if (rc == 0) {
				break;
			} else if (rc == -1) {
				vk_error();
			}

			for (i = 0; i < HTTP_HEADER_COUNT; i++) {
				if (strcasecmp(self->key, http_headers[i].repr) == 0) {
					switch (http_headers[i].http_header) {
						case TRANSFER_ENCODING:
							if (strcasecmp(self->val, "chunked") == 0) {
								self->request.chunked = 1;
							}
							break;
						case CONTENT_LENGTH:
							self->request.content_length = dec_size(self->val);
							break;
						case TRAILER:
							break;
						case TE:
							break;
					}
				}
			}

			if (self->request.header_count >= 15) { 
				continue;
			}
			copy_into(self->request.headers[self->request.header_count].key, self->key);
			copy_into(self->request.headers[self->request.header_count].val, self->val);

			++self->request.header_count;
		}

		vk_request(&self->response_vk, self->return_ft, &self->request, self->response);

		/* request entity */
		if (self->request.content_length > 0) {
			/* fixed size */
			vk_dbg("Fixed entity of size %zu:\n", self->request.content_length);
			for (self->to_receive = self->request.content_length; self->to_receive > 0; self->to_receive -= sizeof (self->chunk.buf)) {
				self->chunk.size = self->to_receive > sizeof (self->chunk.buf) ? sizeof (self->chunk.buf) : self->to_receive;
				vk_read(rc, self->chunk.buf, self->chunk.size);
				if (rc != self->chunk.size) {
					vk_raise(EPIPE);
				}
				vk_dbg("Chunk of size %zu:\n%.*s", self->chunk.size, (int) self->chunk.size, self->chunk.buf);
				vk_write(self->chunk.buf, self->chunk.size);
			}
			vk_hup();
			vk_flush();
		} else if (self->request.chunked) {
			/* chunked */

			do {
				vk_read_chunk_proto(rc, self->chunk);
				if (rc == 0) {
					vk_dbg("%s", "End of chunks.\n");
					break;
				}
				vk_dbg("Chunk of size %zu:\n%.*s", self->chunk.size, (int) self->chunk.size, self->chunk.buf);
				vk_write(self->chunk.buf, self->chunk.size);
			} while (self->chunk.size > 0);

			vk_hup();
			vk_flush();

			/* request trailers */
			for (;;) {
				vk_readrfcheader(rc, self->line, sizeof (self->line) - 1, &self->key, &self->val);
				if (rc == 0) {
					break;
				} else if (rc == -1) {
					vk_error();
				}

				for (i = 0; i < HTTP_TRAILER_COUNT; i++) {
					if (strcasecmp(self->key, http_trailers[i].repr) == 0) {
						switch (http_trailers[i].http_trailer) {
							case POST_CONTENT_LENGTH:
								self->request.content_length = dec_size(self->val);
								break;
						}
					}
				}

				if (self->request.header_count >= 15) { 
					continue;
				}
				copy_into(self->request.headers[self->request.header_count].key, self->key);
				copy_into(self->request.headers[self->request.header_count].val, self->val);

				++self->request.header_count;
			}
		} else if (self->request.method == PRI && self->request.version == HTTP20) {
			/* HTTP/2.0 Connection Preface */
			vk_readline(rc, self->line, sizeof (self->line) - 1);
			if (rc == 0) {
				break;
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';
			if (rc == 2 && self->line[0] == 'S' && self->line[1] == 'M') {
				/* is HTTP/2.0 */
			} else {
				vk_raise(EINVAL);
			}

			vk_readline(rc, self->line, sizeof (self->line) - 1);
			if (rc != 0) {
				vk_error();
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';

			if (rc != 0) {
				vk_raise(EINVAL);
			}
		} else {
			/* no entity, not chunked */
			vk_hup();
			vk_flush();
		}

		vk_dbg("%s", "end of request\n");

	} while (!vk_nodata());

	vk_finally();
	if (errno != 0) {
		vk_perror("request error");
	}
	vk_end();
}

#include <fcntl.h>

int main(int argc, char *argv[]) {
	int rc;
	struct that that;

	if (argc >= 2) {
		rc = open(argv[1], O_RDONLY);
	} else {
		rc = open("http_request_pipeline.txt", O_RDONLY);
	}

	memset(&that, 0, sizeof (that));
	VK_INIT_PRIVATE(rc, &that, http11_request, vk_sync_unblock, rc, 1, 4096 * 13);
	if (rc == -1) {
		return 1;
	}

	do {
		vk_run(&that);
	} while (vk_runnable(&that));;

	rc = vk_deinit(&that);
	if (rc == -1) {
		return 2;
	}

	return 0;
}


