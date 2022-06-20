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

#include "vk_future_s.h"

void http11_response(struct that *that) {
	int rc = 0;

	struct {
		struct rfcchunk chunk;
		struct vk_future request_ft;
		struct request *request_ptr;
		int error_cycle;
	} *self;

	vk_begin_pipeline(&self->request_ft);

	self->error_cycle = 0;

	for (;;) {
		/* get request */
		vk_listen(&self->request_ft);
		self->request_ptr = vk_future_get(&self->request_ft);

		/* set request receipt */
		vk_future_resolve(&self->request_ft, 0);
		vk_respond(&self->request_ft);

		/* write response header to socket */
		rc = snprintf(self->chunk.buf, sizeof (self->chunk.buf) - 1, "200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n");
		if (rc == -1) {
			vk_error();
		}
		vk_write(self->chunk.buf, strlen(self->chunk.buf));

		if (self->request_ptr->content_length > 0 || self->request_ptr->chunked) {
			/* write chunks */
			while (!vk_nodata()) {
				vk_readrfcchunk(rc, self->chunk);
				vk_dbg("chunk.size = %zu: %.*s\n", self->chunk.size, (int) self->chunk.size, self->chunk.buf);
				vk_writerfcchunk_proto(rc, self->chunk);
			}
			vk_clear();
		} else {
			vk_dbg("%s\n", "no entity expected");
			/* no entity */
			if (vk_nodata()) {
				vk_clear();
				vk_dbg("%s\n", "cleared for next response");
			}
		}

		vk_write("0\r\n\r\n", 5);
		vk_flush();
		
		vk_dbg("%s\n", "end of response");
	}

	vk_finally();
	if (errno != 0) {
		vk_perror("response error");
		if (self->error_cycle < 2) {
			self->error_cycle++;

			rc = snprintf(self->chunk.buf, sizeof (self->chunk.buf) - 1, "%i OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s", errno == EINVAL ? 400 : 500, strlen(strerror(errno)), strerror(errno));
			if (rc == -1) {
				vk_error();
			}
			vk_write(self->chunk.buf, strlen(self->chunk.buf));
			vk_flush();
			vk_dbg("%s\n", "closing FD");
			vk_tx_close();
			vk_dbg("%s\n", "FD closed");
		}
	}
	vk_dbg("%s\n", "end of response handler");

	vk_end();
}

#include "vk_state_s.h"
void http11_request(struct that *that) {
	int rc = 0;
	int i;

	struct {
		struct rfcchunk chunk;
		ssize_t to_receive;
		int rc;
		char line[1024];
		char *tok;
		char *key;
		char *val;
		struct vk_future return_ft;
		struct request request;
		void *response;
		struct that response_vk;
	} *self;

	vk_begin();

	vk_child(&self->response_vk, http11_response);

	vk_request(&self->response_vk, &self->return_ft, NULL, self->response);
	if (self->response != 0) {
		vk_error();
	}

	do {
		/* request line */
		vk_readrfcline(rc, self->line, sizeof (self->line) - 1);
		self->line[rc] = '\0';
		if (rc == 0) {
			vk_raise_at(&self->response_vk, EINVAL);
			vk_raise(EINVAL);
		} else if (rc == -1) {
			vk_error_at(&self->response_vk);
			vk_error();
		}

		/* request method */
		self->request.method = NO_METHOD;
		self->tok = self->line;
		if (self->tok == NULL) {
			vk_raise_at(&self->response_vk, EINVAL);
			vk_raise(EINVAL);
		}
		self->val = strsep(&self->tok, " \t");
		for (i = 0; i < METHOD_COUNT; i++) {
			if (strcasecmp(self->val, methods[i].repr) == 0) {
				self->request.method = methods[i].method;
				break;
			}
		}

		/* request URI */
		if (self->tok == NULL) {
			vk_raise_at(&self->response_vk, EINVAL);
			vk_raise(EINVAL);
		}
		self->val = strsep(&self->tok, " \t");
		copy_into(self->request.uri, self->val);

		/* request version */
		if (self->tok == NULL) {
			vk_raise_at(&self->response_vk, EINVAL);
			vk_raise(EINVAL);
		}
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
				vk_error_at(&self->response_vk);
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
							vk_dbg("content-length: %zu\n", self->request.content_length);
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
			if (self->key == NULL) {
				abort();
			}
			if (self->val == NULL) {
				abort();
			}
			copy_into(self->request.headers[self->request.header_count].key, self->key);
			copy_into(self->request.headers[self->request.header_count].val, self->val);

			++self->request.header_count;
		}

		vk_request(&self->response_vk, &self->return_ft, &self->request, self->response);

		/* request entity */
		if (self->request.content_length > 0) {
			/* fixed size */
			vk_dbg("Fixed entity of size %zu:\n", self->request.content_length);
			for (self->to_receive = self->request.content_length; self->to_receive > 0; self->to_receive -= sizeof (self->chunk.buf)) {
				self->chunk.size = self->to_receive > sizeof (self->chunk.buf) ? sizeof (self->chunk.buf) : self->to_receive;
				vk_read(rc, self->chunk.buf, self->chunk.size);
				if (rc != self->chunk.size) {
					vk_raise_at(&self->response_vk, EPIPE);
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
				vk_readrfcchunk_proto(rc, self->chunk);
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
					vk_error_at(&self->response_vk);
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
				vk_raise_at(&self->response_vk, EINVAL);
				vk_raise(EINVAL);
			}

			vk_readline(rc, self->line, sizeof (self->line) - 1);
			if (rc != 0) {
				vk_error_at(&self->response_vk);
				vk_error();
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';

			if (rc != 0) {
				vk_raise_at(&self->response_vk, EINVAL);
				vk_raise(EINVAL);
			}
		} else {
			/* no entity, not chunked */
			vk_dbg("%s\n", "no entity");
			vk_hup();
			vk_flush();
			vk_dbg("%s\n", "clear for next request");
		}

		vk_dbg("%s\n", "end of request");

	} while (!vk_nodata());

	vk_raise_at(&self->response_vk, 0);

	vk_finally();
	if (errno != 0) {
		vk_perror("request error");
	}

	vk_dbg("%s\n", "end of request handler");
	vk_end();
}

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
struct vk_server {
	int domain;
	int type;
	int protocol;

	struct sockaddr *address;
	socklen_t address_len;

	int backlog;
};
void listener(struct that *that) {
	int rc = 0;
	struct {
		struct vk_future ft_arg;
		struct vk_server *server_ptr;
		int socket_fd;
		int accepted_fd;
		struct vk_pipe *accepted_pipe_ptr;
		struct sockaddr client_address;
		socklen_t client_address_len;
		int opt;
		struct that request_vk;
	} *self;
	vk_begin();

	vk_get_request(&self->ft_arg);
	self->server_ptr = self->ft_arg.msg;

	rc = socket(self->server_ptr->domain, self->server_ptr->type, self->server_ptr->protocol);
	if (rc == -1) {
		vk_error();
	}
	self->socket_fd = rc;

	self->opt = 1;
	rc = setsockopt(self->socket_fd, SOL_SOCKET, SO_REUSEADDR, &self->opt, sizeof (self->opt));
	if (rc == -1) {
		vk_error();
	}

	rc = bind(self->socket_fd, self->server_ptr->address, self->server_ptr->address_len);
	if (rc == -1) {
		vk_error();
	}

	rc = listen(self->socket_fd, self->server_ptr->backlog);
	if (rc == -1) {
		vk_error();
	}

	self->accepted_pipe_ptr = vk_socket_get_rx_fd(vk_get_socket(that));
	vk_pipe_init_fd(self->accepted_pipe_ptr, self->socket_fd);

	for (;;) {
		vk_socket_enqueue_blocked(vk_get_socket(that));
		vk_wait(vk_get_socket(that));
		rc = accept(self->socket_fd, &self->client_address, &self->client_address_len);
		if (rc == -1) {
			vk_error();
		}
		self->accepted_fd = rc;

		fcntl(self->accepted_fd, F_SETFL, O_NONBLOCK);

		vk_accepted(&self->request_vk, http11_request, self->accepted_fd, self->accepted_fd);
		vk_play(&self->request_vk);
	}

	vk_finally();
	if (errno) {
		vk_perror("listener");
	}

	vk_end();
}

#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "vk_heap.h"
#include "vk_kern.h"
#include "vk_proc.h"

int main(int argc, char *argv[]) {
	int rc;
	int rx_fd;
	struct vk_heap_descriptor *kern_heap_ptr;
	struct vk_kern *kern_ptr;
	struct vk_proc *proc_ptr;
	struct that *vk_ptr;

	struct vk_server server;
	struct sockaddr_in address;
	struct vk_future ft;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	server.domain = PF_INET;
	server.type = SOCK_STREAM;
	server.protocol = 0;
	server.address = (struct sockaddr *) &address;
	server.address_len = sizeof (address);
	server.backlog = 128;

	kern_heap_ptr = calloc(1, vk_heap_alloc_size());
	kern_ptr = vk_kern_alloc(kern_heap_ptr);
	if (kern_ptr == NULL) {
		return 1;
	}

	proc_ptr = vk_kern_alloc_proc(kern_ptr);
	if (proc_ptr == NULL) {
		return 1;
	}
	rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * /*24*/ 34);
	if (rc == -1) {
		return 1;
	}

	vk_ptr = vk_proc_alloc_that(proc_ptr);
	if (vk_ptr == NULL) {
		return 1;
	}

	ft.msg = &server;
	ft.vk = NULL;
	ft.error = 0;
	ft.next = NULL;

	if (argc >= 2) {
		rc = open(argv[1], O_RDONLY);
	} else {
		rc = open("http_request_pipeline.txt", O_RDONLY);
	}
	rx_fd = rc;
	fcntl(rx_fd, F_SETFL, O_NONBLOCK);
	fcntl(0,  F_SETFL, O_NONBLOCK);

	VK_INIT(vk_ptr, proc_ptr, listener, rx_fd, 1);
	vk_set_future(vk_ptr, &ft);

	vk_proc_enqueue_run(proc_ptr, vk_ptr);

	vk_kern_flush_proc_queues(kern_ptr, proc_ptr);

	rc = vk_kern_loop(kern_ptr);
	if (rc == -1) {
		return -1;
	}

	rc = vk_deinit(vk_ptr);
	if (rc == -1) {
		return 4;
	}

	rc = vk_proc_deinit(proc_ptr);
	if (rc == -1) {
		return 5;
	}

	vk_kern_free_proc(kern_ptr, proc_ptr);

	return 0;
}


