/* Copyright 2022 BCW. All Rights Reserved. */
#include <stdlib.h> /* abort */

#include "vk_debug.h"
#include "vk_future_s.h" /* for `struct vk_future` */
#include "vk_fetch_s.h"
#include "vk_fetch.h"
#include "vk_http11.h"
#include "vk_rfc.h"
#include "vk_rfc_s.h" /* for `struct vk_rfcchunk` */
#include "vk_signal.h"

void http11_response(struct vk_thread* that)
{
	ssize_t rc = 0;

	struct {
		struct vk_service* service_ptr; /* via http11_request via vk_copy_arg() */
		struct vk_rfcchunk chunk;
		struct request request;
	}* self;

	vk_begin();

	self->service_ptr = vk_future_get(vk_ft_dequeue(that));

	do {
		/* get request */
		vk_read(rc, (char*)&self->request, sizeof(self->request));
		if (rc != sizeof(self->request)) {
			errno = EPIPE;
			vk_error();
		}

		/* write response header to socket (if past HTTP/0.9) */
		if (self->request.version == HTTP09) {
			/* HTTP/0.9 */

			/* body only */
			vk_write_literal("<html>\n"
					 "\t<head>\n"
					 "\t\t<title>HTTP/0.9 response</title>\n"
					 "\t</head>\n"
					 "\t<body>\n"
					 "\t\t<h1>HTTP/0.9 response</h1>\n"
					 "\t\t<p>This is an example response.</p>\n"
					 "\t</body>\n"
					 "</html>\n");
		} else if (self->request.version == HTTP10) {
			/* HTTP/1.0 */

			/* header */
			vk_write_literal("HTTP/1.0 200 OK\r\n"
					 "Content-Type: text/plain\r\n");

			if (self->request.method == GET) {
				vk_write_literal("Content-Length: 14\r\n");
			} else {
				vk_writef(rc, vk_rfcchunk_get_buf(&self->chunk), vk_rfcchunk_get_buf_size(&self->chunk),
					  "Content-Length: %zu\r\n", self->request.content_length);
				if (rc == -1) {
					vk_error();
				}
			}

			vk_write_literal("\r\n");

			/* body */
			if (self->request.method == GET) {
				vk_write_literal("Hello, World!\n");
			} else {
				if (self->request.content_length > 0) {
					/* write entity by splicing from stdin to stdout */
					vk_forward(rc, self->request.content_length);
					if (rc == -1) {
						vk_error();
					} else if (rc < self->request.content_length) {
						errno = EPIPE;
						vk_error();
					}
				}
			}
		} else {
			/* HTTP/1.1 */

			/* header */
			vk_write_literal("HTTP/1.1 200 OK\r\n"
					 "Content-Type: text/plain\r\n");
			if (self->request.chunked) {
				vk_write_literal("Transfer-Encoding: chunked\r\n");
			}
			if (self->request.close) {
				vk_write_literal("Connection: close\r\n");
			}

			/* body */
			if (self->request.method == GET) {
				if (self->request.chunked) {
					vk_write_literal("d\r\nHello, World!\r\n");
				} else {
					vk_write_literal("Content-Length: 14\r\n\r\nHello, World!\n");
				}
			} else {
				if (self->request.chunked) {
					vk_write_literal("\r\n");
					/* write chunks */
					while (!vk_pollhup()) {
						vk_readrfcchunk(rc, &self->chunk);
						if (rc == -1) {
							vk_error();
						}
						vk_dbgf("chunk.size = %zu: %.*s\n", self->chunk.size,
							(int)self->chunk.size, self->chunk.buf);
						vk_writerfcchunk_proto(rc, &self->chunk);
					}
					vk_readhup();

					vk_writerfcchunkend_proto();
				} else if (self->request.content_length > 0) {
					vk_writef(rc, vk_rfcchunk_get_buf(&self->chunk),
						  vk_rfcchunk_get_buf_size(&self->chunk), "Content-Length: %zu\r\n",
						  self->request.content_length);
					if (rc == -1) {
						vk_error();
					}
					vk_write_literal("\r\n");
					/* write entity by splicing from stdin to stdout */
					vk_dbgf("splicing %zu bytes for fixed-sized entity\n",
						self->request.content_length);
					vk_forward(rc, self->request.content_length);
					if (rc < self->request.content_length) {
						errno = EPIPE;
						vk_error();
					}
				}
			}
		}
		/* vk_flush(); -- let flush be lazy, on read block */

		vk_dbg("end of response");
	} while (!self->request.close);

	vk_flush(); /* flush before closing -- closing request will not yet be flushed */
	vk_dbg("closing write-side");
	/* vk_tx_shutdown(); */
	vk_tx_close();
	vk_dbg("closed");

	errno = 0;
	vk_finally();
	if (errno != 0) {
		vk_dbg_perror("response error");
		vk_sigerror();
		rc = snprintf(self->chunk.buf, sizeof(self->chunk.buf) - 1,
			      "HTTP/1.1 %i %s\r\nContent-Type: text/plain\r\nContent-Length: "
			      "%zu\r\nConnection: close\r\n\r\n%s\n",
			      errno == EINVAL ? 400 : 500,
			      errno == EINVAL ? "Bad Request" : "Internal Server Error",
			      strlen(strerror(errno)) + 1, strerror(errno));
		if (rc == -1) {
			vk_perror("snprintf");
		}
		vk_write(self->chunk.buf, rc);
		vk_flush();
		vk_tx_close();
	}

	vk_free(); /* deallocation to pair with the allocation of this child thread */
	vk_dbg("end of response handler");

	vk_end();
}

#include "vk_service.h"

void http11_request(struct vk_thread* that)
{
	ssize_t rc = 0;

	struct {
		struct vk_service service; /* via vk_copy_arg() */
		struct vk_rfcchunk chunk;
		ssize_t to_receive;
		int rc;
		char line[1024];
		char* tok;
		char* key;
		char* val;
		enum HTTP_HEADER header;
		enum HTTP_TRAILER trailer;
		struct vk_future request_ft;
		struct request request;
		struct vk_thread* response_vk_ptr;
	}* self;

	vk_begin();

	vk_dbgf("http11_request() from client %s:%s to server %s:%s\n",
		vk_accepted_get_address_str(&self->service.accepted), vk_accepted_get_port_str(&self->service.accepted),
		vk_server_get_address_str(&self->service.server), vk_server_get_port_str(&self->service.server));
	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());

	vk_responder(self->response_vk_ptr, http11_response);
	vk_send(self->response_vk_ptr, &self->request_ft, &self->service);

	do {
		vk_dbg("start of request");

		/* request line */
		vk_readrfcline(rc, self->line, sizeof(self->line) - 1);
		if (rc == 0) {
			vk_dbg("empty request line -- aborting");
			break;
		} else if (rc == -1) {
			vk_error();
		}
		self->line[rc] = '\0';
		vk_dbgf("Request Line: %s\n", self->line);

		/* request line -- prepared for strsep parsing */
		self->tok = self->line;

		/* get request method value */
		self->val = strsep(&self->tok, " \t");
		if (self->val == NULL) {
			vk_raise(EINVAL);
		}
		self->request.method = string_to_HTTP_METHOD(self->val);

		/* get request URI value */
		self->val = strsep(&self->tok, " \t");
		if (self->val == NULL) {
			vk_raise(EINVAL);
		}
		copy_into(self->request.uri, self->val);

		/* get request version value */
		self->val = strsep(&self->tok, " \t");
		if (self->val == NULL) {
			self->request.version = HTTP09; /* default for when no version token is specified */
		} else {
			self->request.version = string_to_HTTP_VERSION(self->val);
		}

		/* request headers */
		self->request.chunked = 0;
		self->request.content_length = 0;
		self->request.header_count = 0;
		switch (self->request.version) {
			case NO_VERSION:
			case HTTP09:
			case HTTP10:
				self->request.close = 1;
				break;
			case HTTP11:
			case HTTP20:
				self->request.close = 0;
				break;
		}
		while (self->request.version != HTTP09) {
			vk_readrfcheader(rc, self->line, (ssize_t)sizeof(self->line) - 1, &self->key, &self->val);
			if (rc == 0) {
				break;
			} else if (rc == -1) {
				vk_error();
			}
			vk_dbgf("Header: %s: %s\n", self->key, self->val);

			self->header = string_to_HTTP_HEADER(self->key);
			switch (self->header) {
				case TRANSFER_ENCODING:
					if (strcasecmp(self->val, "chunked") == 0) {
						self->request.chunked = 1;
					}
					break;
				case CONTENT_LENGTH:
					self->request.content_length = dec_size(self->val);
					vk_dbgf("content-length: %zu\n", self->request.content_length);
					break;
				case CONNECTION:
					if (strcasecmp(self->val, "close") == 0) {
						self->request.close = 1;
						vk_dbgf("%s\n", "closing connection");
					}
					break;
				case TRAILER:
				case TE:
				default:
					break;
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

		if (strncmp(self->request.uri, "/crash", strlen("/crash")) == 0) {
			rc = 0;
			rc = 5 / rc; /* trigger SIGILL */
		}

		vk_write((char*)&self->request, sizeof(self->request));

		/* request entity */
		if (self->request.content_length > 0) {
			/* fixed size */

			vk_dbgf("Fixed entity of size %zu:\n", self->request.content_length);
			vk_forward(rc, self->request.content_length);
			if (rc < self->request.content_length) {
				errno = EPIPE;
				vk_error();
			}
			vk_flush();
		} else if (self->request.chunked) {
			/* chunked */
			vk_dbgf("%s", "Chunked entity:\n");

			do {
				vk_readrfcchunk_proto(rc, &self->chunk);
				if (rc == 0) {
					vk_dbgf("%s", "End of chunks.\n");
					break;
				}
				vk_dbgf("Chunk of size %zu:\n%.*s\n", self->chunk.size, (int)self->chunk.size,
					self->chunk.buf);
				vk_writerfcchunk(&self->chunk);
			} while (self->chunk.size > 0);
			vk_hup();

			/* request trailers */
			for (;;) {
				vk_readrfcheader(rc, self->line, (ssize_t)sizeof(self->line) - 1, &self->key,
						 &self->val);
				if (rc == 0) {
					break;
				} else if (rc == -1) {
					vk_error();
				}

				self->trailer = string_to_HTTP_TRAILER(self->key);

				switch (self->trailer) {
					case POST_CONTENT_LENGTH:
						self->request.content_length = dec_size(self->val);
						break;
					default:
						break;
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
			vk_readline(rc, self->line, sizeof(self->line) - 1);
			if (rc == 0) {
				break;
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';
			if (rc == 2 && self->line[0] == 'S' && self->line[1] == 'M') {
				/* is HTTP/2.0 */
			} else {
				/* response is waiting on vk_read() */
				vk_raise(EINVAL);
			}

			vk_readline(rc, self->line, sizeof(self->line) - 1);
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
			vk_dbg("no entity");
			vk_flush();
		}

		vk_dbg("end of request");
		if (self->request.close) {
			vk_dbg("closing connection");
		}
	} while (!vk_nodata() && !self->request.close);

	/*
	vk_dbg("closing read-side");
	vk_rx_shutdown();
	vk_dbg("closed");
	*/

	vk_flush();

	errno = 0;
	vk_finally();
	vk_call(self->response_vk_ptr);
	if (errno != 0) {
		vk_sigerror();
		if (errno == EPIPE) {
			vk_dbg_perror("request_error");
		} else {
			vk_perror("request_error");
		}
	}

	vk_dbg("end of request handler");
	vk_end();
}

