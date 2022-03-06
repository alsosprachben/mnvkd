#include "vk_rfc.h"
#include <stdlib.h> /* abort */

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
};
struct response {
	char entity[4096];
	char mime_type[32];
};

void http11_response(struct that *that) {
	int rc;

	struct {
		struct future request_ft;
		struct request *request_ptr;
		char response[1024];
	} *self;

	dprintf(2, "response handler starting...\n");
	vk_begin();

	dprintf(2, "response handler initialized\n");

	for (;;) {
		/* get request */
		vk_listen(self->request_ft);
		self->request_ptr = future_get(self->request_ft);
		dprintf(2, "Got request.\n");
		future_resolve(self->request_ft, 0);
		vk_respond(self->request_ft);
		dprintf(2, "Responded.\n");

		rc = snprintf(self->response, sizeof (self->response) - 1, "200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s", strlen("test"), "test");
		if (rc == -1) {
			vk_error();
		}

		vk_write(self->response, strlen(self->response));
		vk_flush();
		dprintf(2, "Wrote response: %s", self->response);
	}

	vk_finally();

	vk_end();
}

void http11_request(struct that *that) {
	int rc;
	int i;

	struct {
		char chunk[4096];
		size_t chunk_size;
		size_t content_length;
		ssize_t to_receive;
		int rc;
		char line[1024];
		char *tok;
		char *key;
		char *val;
		struct future return_ft;
		struct request request;
		intptr_t response;
		int chunked;
		struct that response_vk;
	} *self;

	vk_begin();

	VK_INIT_CHILD(rc, that, &self->response_vk, http11_response, that->socket.rx_fd, that->socket.tx_fd, 4096 * 1);
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
		self->chunked = 0;
		self->content_length = 0;
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
								dprintf(2, "chunked encoding\n");
								self->chunked = 1;
							}
							break;
						case CONTENT_LENGTH:
							self->content_length = dec_size(self->val);
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
		if (self->content_length > 0) {
			/* fixed size */
			if (self->content_length <= sizeof (self->chunk) - 1) {
				vk_read(self->chunk, self->content_length);
				self->chunk[self->content_length] = '\0';
				dprintf(2, "Small whole entity of size %zu:\n%s", self->content_length, self->chunk);
			} else {
				dprintf(2, "Large entity of size %zu:\n", self->content_length);
				for (self->to_receive = self->content_length; self->to_receive > 0; self->to_receive -= sizeof (self->chunk)) {
					self->chunk_size = self->to_receive > sizeof (self->chunk) ? sizeof (self->chunk) : self->to_receive;
					vk_read(self->chunk, self->chunk_size);
					dprintf(2, "Chunk of size %zu:\n%.*s", self->chunk_size, (int) self->chunk_size, self->chunk);
				}
			}
		} else if (self->chunked) {
			/* chunked */

			do {
				vk_readrfcline(rc, self->line, sizeof (self->line) - 1);
				if (rc == 0) {
					break;
				}
				self->line[rc] = '\0';
				if (rc == 0) {
					break;
				}

				self->chunk_size = hex_size(self->line);
				if (self->chunk_size == 0) {
					break;
				}
				vk_read(self->chunk, self->chunk_size);
				dprintf(2, "Chunk of size %zu:\n%.*s", self->chunk_size, (int) self->chunk_size, self->chunk);
			} while (self->chunk_size > 0);

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
								self->content_length = dec_size(self->val);
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
				vk_error();
			}

			vk_readline(rc, self->line, sizeof (self->line) - 1);
			if (rc != 0) {
				vk_error();
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';

			if (rc != 0) {
				vk_error();
			}
		}

	} while (!vk_eof());

	vk_finally();

	vk_end();
}

int main() {
	int rc;
	struct that that;

	memset(&that, 0, sizeof (that));
	VK_INIT_PRIVATE(rc, &that, http11_request, 0, 1, 4096 * 12);
	if (rc == -1) {
		return 1;
	}

	do {
		vk_run(&that);
		/*
		rc = vk_sync_unblock(&that);
		if (rc == -1) {
			return -1;
		}
		*/
	} while (vk_runnable(&that));;

	rc = vk_deinit(&that);
	if (rc == -1) {
		return 2;
	}

	return 0;
}


