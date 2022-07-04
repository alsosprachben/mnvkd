#ifndef VK_HTTP11_H
#define VK_HTTP11_H

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
    CONNECTION,
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
    { CONNECTION, "Connection", },
    { TRAILER, "Trailer", },
	{ TE, "TE", },
};
#define HTTP_HEADER_COUNT 5

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
    int close;
};

#endif