#include "vk_state.h"

#include <strings.h>

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
	{ HEAD,    "HEAD",    },
	{ POST,    "POST",    },
	{ PUT,     "PUT",     },
	{ DELETE,  "DELETE",  },
	{ CONNECT, "CONNECT", },
	{ OPTIONS, "OPTIONS", },
	{ TRACE,   "TRACE",   },
	{ PATCH,   "PATCH",   },
};
#define METHOD_COUNT 8

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
	char *key;
	char *val;
};



void http11(struct that *that) {
	int rc;
	char *tok;
	int i;
	int c;

	struct {
		int rc;
		char line[1024];
		char *key;
		char *val;
		enum HTTP_METHOD method;
		char uri[1024];
		enum HTTP_VERSION version;
		struct header headers[32];
		int header_count;
		size_t content_length;
		int chunked;
	} *self;

	vk_begin();

	for (;;) {
		/* request line */
		vk_readline(rc, self->line, sizeof (self->line) - 1);
		if (rc == 0) {
			vk_error();
		}
		rtrim(self->line, &rc);
		self->line[rc] = '\0';

		/* request method */
		self->method = NO_METHOD;
		tok = self->line;
		self->val = strsep(&tok, " \t");
		for (i = 0; i < METHOD_COUNT; i++) {
			if (strcasecmp(self->val, methods[i].repr) == 0) {
				self->method = methods[i].method;
				break;
			}
		}

		/* request URI */
		self->val = strsep(&tok, " \t");
		strncpy(self->uri, self->val, sizeof (self->uri) - 1);
		self->uri[sizeof (self->uri)] = '\0';

		/* request version */
		self->version = NO_VERSION;
		self->val = strsep(&tok, " \t");
		for (i = 0; i < VERSION_COUNT; i++) {
			if (strcasecmp(self->val, versions[i].repr) == 0) {
				self->version = versions[i].version;
				break;
			}
		}

		/* request headers */
		self->chunked = 0;
		self->content_length = 0;
		do {
			vk_readline(rc, self->line, sizeof (self->line) - 1);
			if (rc == 0) {
				vk_error();
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';

			tok = self->line;
			self->key = strsep(&tok, ":");
			if (self->key != NULL) {
				c = strlen(tok);
				self->val = ltrim(tok, &c);
			}

			if (self->val == NULL) {
				continue;
			}

			c = strlen(self->key);
			vk_calloc(self->headers[self->header_count].key, c);
			strncpy(self->headers[self->header_count].key, self->key, c);

			c = strlen(self->val);
			vk_calloc(self->headers[self->header_count].val, c);
			strncpy(self->headers[self->header_count].val, self->val, c);

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
		} while (rc > 0 && ++self->header_count < 32);

	}

	vk_end();
}

int main() {
	int rc;
	struct that that;

	rc = VK_INIT_PRIVATE(&that, http11, 0, 1, 4096 * 2);
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


