#include "vk_state.h"

#include <strings.h>
#include <stdlib.h> /* abort */

/* from the return of vk_readline(), trim the newline, and adjust size */
void rtrim(char *line, int *size_ptr) {
	if (*size_ptr < 1) {
		return;
	}
	if (    line[*size_ptr - 1] == '\n') {
		line[*size_ptr - 1] = '\0';;
		--(*size_ptr);
	}
	if (*size_ptr < 1) {
		return;
	}
	if (    line[*size_ptr - 1] == '\r') {
		line[*size_ptr - 1] = '\0';
		--(*size_ptr);
	}
}

/* trim spaces from the left by returning the start of the string after the prefixed spaces */
char *ltrim(char *line, int *size_ptr) {
	while (*size_ptr > 0 && line[0] == ' ') {
		++line;
		--(*size_ptr);
	}
	return line;
}

/* From the BSD man page for strncpy */
#define copy_into(buf, input) { \
	(void)strncpy(buf, input, sizeof (buf)); \
	buf[sizeof(buf) - 1] = '\0'; \
}

/* branchless int to hex-char */
#define VALHEX(v) ((((v) + 48) & (-((((v) - 10) & 0x80) >> 7))) | (((v) + 55) & (-(((9 - (v)) & 0x80) >> 7))))
size_t hex_size(char *val) {
	size_t size;
	int i;

	for (i = 0; i < 16 && val[i] != '\0'; i++) {
		size |= VALHEX(val[i]) << (4 * i); 
	}

	return size;
}

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
};
#define METHOD_COUNT 9

enum HTTP_VERSION {
	NO_VERSION,
	HTTP09,
	HTTP10,
	HTTP11,
	HTTP2,
};
struct http_version {
	enum HTTP_VERSION version;
	char *repr;
};
static struct http_version versions[] = {
	{ HTTP09, "HTTP/0.9", },
	{ HTTP10, "HTTP/1.0", },
	{ HTTP11, "HTTP/1.1", },
	{ HTTP2,  "HTTP/2.0", },
};
#define VERSION_COUNT 4

enum HTTP_HEADER {
	TRANSFER_ENCODING,
	CONTENT_LENGTH,
};
struct http_header {
	enum HTTP_HEADER http_header;
	char *repr;
};
static struct http_header http_headers[] = {
	{ TRANSFER_ENCODING, "Transfer-Encoding", },
	{ CONTENT_LENGTH, "Content-Length", },
};
#define HTTP_HEADER_COUNT 2

struct header {
	char key[32];
	char val[96];
};



void http11(struct that *that) {
	int rc;
	int i;

	struct {
		int rc;
		char line[1024];
		char *tok;
		char *key;
		char *val;
		enum HTTP_METHOD method;
		char uri[1024];
		enum HTTP_VERSION version;
		struct header headers[15];
		int header_count;
		size_t content_length;
		char *entity;
		int chunked;
	} *self;

	vk_begin();

	do {
		/* request line */
		vk_readline(rc, self->line, sizeof (self->line) - 1);
		if (rc == 0) {
			break;
		}
		rtrim(self->line, &rc);
		self->line[rc] = '\0';
		if (rc == 0) {
			break;
		}

		/* request method */
		self->method = NO_METHOD;
		self->tok = self->line;
		self->val = strsep(&self->tok, " \t");
		for (i = 0; i < METHOD_COUNT; i++) {
			if (strcasecmp(self->val, methods[i].repr) == 0) {
				self->method = methods[i].method;
				break;
			}
		}

		/* request URI */
		self->val = strsep(&self->tok, " \t");
		copy_into(self->uri, self->val);

		/* request version */
		self->version = NO_VERSION;
		self->val = strsep(&self->tok, " \t");
		for (i = 0; i < VERSION_COUNT; i++) {
			if (strcasecmp(self->val, versions[i].repr) == 0) {
				self->version = versions[i].version;
				break;
			}
		}

		/* request headers */
		self->chunked = 0;
		self->content_length = 0;
		self->header_count = 0;
		do {
			vk_readline(rc, self->line, sizeof (self->line) - 1);
			if (rc == 0) {
				vk_error();
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';
			if (rc == 0) {
				/* end of headers */
				continue;
			}

			self->tok = self->line;
			self->key = strsep(&self->tok, ": \t");
			if (self->key == NULL) {
				continue;
			}

			if (self->tok != NULL) {
				self->val = self->tok;
			}

			if (self->val == NULL) {
				continue;
			}

			copy_into(self->headers[self->header_count].key, self->key);
			copy_into(self->headers[self->header_count].val, self->val);

			for (i = 0; i < HTTP_HEADER_COUNT; i++) {
				if (strcasecmp(self->key, http_headers[i].repr) == 0) {
					switch (http_headers[i].http_header) {
						case TRANSFER_ENCODING:
							if (strcasecmp(self->val, "chunked") == 0) {
								self->chunked = 1;
							}
							break;
						case CONTENT_LENGTH:
							rc = sscanf(self->val, "%zu", &self->content_length);
							if (rc != 1) {
								self->content_length = 0;
							}
							break;
					}
				}
			}

			++self->header_count;
		} while (rc > 0 && ++self->header_count < 15);

		if (self->content_length > 0) {
			vk_calloc(self->entity, self->content_length);
			vk_read(self->entity, self->content_length);
			dprintf(2, "Entity of size %zu:\n%s", self->content_length, self->entity);
			vk_free();
		}

		abort();

	} while (!vk_eof());

	vk_end();
}

int main() {
	int rc;
	struct that that;

	rc = VK_INIT_PRIVATE(&that, http11, 0, 1, 4096);
	if (rc == -1) {
		return 1;
	}

	do {
		vk_run(&that);
		rc = vk_sync_unblock(&that);
		if (rc == -1) {
			return -1;
		}
	} while (vk_runnable(&that));;

	rc = vk_deinit(&that);
	if (rc == -1) {
		return 2;
	}

	return 0;
}


