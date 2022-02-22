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
		line++;
		size_ptr--;
	}
	dprintf(2, "rtrim() = %s\n", line);
	return line;
}
char *ltrimlen(char *line) {
	int size = strlen(line);
	return ltrim(line, &size);
}

/* From the BSD man page for strncpy */
#define copy_into(buf, input) { \
	(void)strncpy(buf, input, sizeof (buf)); \
	buf[sizeof(buf) - 1] = '\0'; \
}

/* branchless hex-char to int */
#define HEXVAL(b)        ((((b) & 0x1f) + (((b) >> 6) * 0x19) - 0x10) & 0xF)
size_t hex_size(char *val) {
	size_t size;
	int i;

	size = 0;
	for (i = 0; i < 16 && val[i] != '\0'; i++) {
		dprintf(2, "%i %c %u\n", i, val[i], HEXVAL(val[i]));
		size <<= 4;
		size |= HEXVAL(val[i]); 
	}

	return size;
}
/* branchless dec-char to int */
#define DECVAL(b)        ((b) - 0x30)
size_t dec_size(char *val) {
	size_t size;
	int i;

	size = 0;
	for (i = 0; i < 20 && val[i] >= '0' && val[i] <= '9'; i++) {
		dprintf(2, "%i %c %u\n", i, val[i], DECVAL(val[i]));
		size *= 10;
		dprintf(2, "size1=%zu\n", size);
		size += DECVAL(val[i]); 
		dprintf(2, "size2=%zu\n", size);
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
		char chunk[4096];
		size_t chunk_size;
		size_t content_length;
		ssize_t to_receive;
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
				break;
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
				self->val = ltrimlen(self->tok);
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
								dprintf(2, "chunked encoding\n");
								self->chunked = 1;
							}
							break;
						case CONTENT_LENGTH:
							self->content_length = dec_size(self->val);
							break;
					}
				}
			}

			++self->header_count;
		} while (rc > 0 && ++self->header_count < 15);

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
				vk_readline(rc, self->line, sizeof (self->line) - 1);
				if (rc == 0) {
					break;
				}
				rtrim(self->line, &rc);
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
		}

	} while (!vk_eof());

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


