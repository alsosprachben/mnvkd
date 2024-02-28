/* Copyright 2022 BCW. All Rights Reserved. */
#include <stdlib.h> /* abort */

#include "vk_http11.h"
#include "vk_rfc.h"
#include "vk_rfc_s.h" /* for `struct vk_rfcchunk` */
#include "vk_debug.h"
#include "vk_future_s.h" /* for `struct vk_future` */
#include "vk_signal.h"

void http11_response(struct vk_thread *that) {
	int rc = 0;

	struct {
		struct vk_rfcchunk chunk;
		struct vk_future *request_ft_ptr;
		struct request *request_ptr;
		int error_cycle;
		size_t splice_cur;
	} *self;

	vk_begin_pipeline(self->request_ft_ptr);

	self->error_cycle = 0;

	do {
		/* get request */
		vk_listen(self->request_ft_ptr);
		self->request_ptr = vk_future_get(self->request_ft_ptr);

		/* set request receipt */
		vk_future_resolve(self->request_ft_ptr, 0);
		vk_respond(self->request_ft_ptr);

		/* write response header to socket (if past HTTP/0.9) */
		if (self->request_ptr->version == HTTP09) {
			vk_write_literal(rc,
			    "<html>\n"
			    "\t<head>\n"
			    "\t\t<title>HTTP/0.9 response</title>\n"
			    "\t</head>\n"
			    "\t<body>\n"
			    "\t\t<h1>HTTP/0.9 response</h1>\n"
			    "\t\t<p>This is an example response.</p>\n"
			    "\t</body>\n"
			    "</html>\n"
			);
			if (rc == -1) {
				vk_error();
			}
        } else if (self->request_ptr->version == HTTP10) {
            vk_write_literal(rc,
                "HTTP/1.0 200 OK\r\n"
                "Content-Type: text/plain\r\n"
            );
            if (rc == -1) {
                vk_error();
            }

            if (self->request_ptr->method == GET) {
                vk_write_literal(rc, "Content-Length: 14\r\n");
                if (rc == -1) {
                    vk_error();
                }
            } else {
                vk_writef(rc, vk_rfcchunk_get_buf(&self->chunk), vk_rfcchunk_get_buf_size(&self->chunk), "Content-Length: %zu\r\n", self->request_ptr->content_length);
                if (rc == -1) {
                    vk_error();
                }
            }

            vk_write_literal(rc, "\r\n");
            if (rc == -1) {
                vk_error();
            }

            if (self->request_ptr->method == GET) {
                vk_write_literal(rc, "Hello, World!\n");
                if (rc == -1) {
                    vk_error();
                }
            } else {
                if (self->request_ptr->content_length > 0) {
                    /* write entity by splicing from stdin to stdout */
                    self->splice_cur = self->request_ptr->content_length;
                    vk_write_splice(rc, self->splice_cur);
                    if (rc == -1) {
                        vk_error();
                    }
                }
            }
		} else {
		    /* HTTP/1.1 */
		    vk_write_literal(rc,
			    "HTTP/1.1 200 OK\r\n"
			    "Content-Type: text/plain\r\n"
			    "Transfer-Encoding: chunked\r\n"
		    );
			if (rc == -1) {
				vk_error();
			}
		    if (self->request_ptr->close) {
                vk_write_literal(rc, "Connection: close\r\n");
                if (rc == -1) {
                    vk_error();
                }
            }

            vk_write_literal(rc, "\r\n");
            if (rc == -1) {
                vk_error();
            }

            if (self->request_ptr->method == GET) {
                vk_write_literal(rc, "d\r\nHello, World!\r\n");
                if (rc == -1) {
                    vk_error();
                }
            } else {
                if (self->request_ptr->content_length > 0) {
                    /* write entity by splicing from stdin to stdout */
                    vk_dbgf("splicing %zu bytes for fixed-sized entity\n", self->request_ptr->content_length);
                    vk_write_splice(rc, self->request_ptr->content_length);
                    if (rc == -1) {
                        vk_error();
                    }
                    vk_clear();
                    vk_dbg("cleared EOF");
                } else {
                    if (self->request_ptr->content_length > 0 || self->request_ptr->chunked) {
                        /* write chunks */
                        while (!vk_nodata()) {
                            vk_readrfcchunk(rc, &self->chunk);
                            if (rc == -1) {
                                vk_error();
                            }
                            vk_dbgf("chunk.size = %zu: %.*s\n", self->chunk.size, (int) self->chunk.size, self->chunk.buf);
                            vk_writerfcchunk_proto(rc, &self->chunk);
                        }
                        vk_clear();
                        vk_dbg("cleared EOF");
                    } else {
                        vk_dbg("no entity expected");
                        /* no entity */
                        if (vk_nodata()) {
                            vk_clear();
                            vk_dbg("cleared EOF");
                        }
                    }
                }
            }

			vk_write_literal(rc, "0\r\n\r\n");
			if (rc == -1) {
				vk_error();
			}
		}
		vk_flush();

		vk_dbg("end of response");
	} while (!self->request_ptr->close);

	errno = 0;
	vk_finally();
	if (errno != 0) {
		vk_perror("response error");
		if (self->error_cycle < 2) {
			self->error_cycle++;

			{
				char errline[256];
				if (errno == EFAULT && vk_get_signal() != 0) {
					/* interrupted by signal */
					rc = vk_snfault(errline, sizeof (errline) - 1);
					if (rc == -1) {
						/* This is safe because it will not lead back to an EFAULT error, so it is not recursive. */
						vk_error();
					}
					vk_clear_signal();
					rc = snprintf(self->chunk.buf, sizeof (self->chunk.buf) - 1, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s\n", strlen(errline) + 1, errline);
				} else {
					/* regular errno error */
					rc = snprintf(self->chunk.buf, sizeof (self->chunk.buf) - 1, "HTTP/1.1 %i %s\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s\n", errno == EINVAL ? 400 : 500, errno == EINVAL ? "Bad Request" : "Internal Server Error", strlen(strerror(errno)) + 1, strerror(errno));
				}
			}
			if (rc == -1) {
				vk_error();
			}
			vk_write(self->chunk.buf, rc);
			vk_flush();
			vk_dbg("closing FD");
			vk_tx_close();
			vk_dbg("FD closed");
		}
	} else {
		vk_dbg("closing FD");
		vk_tx_close();
		vk_dbg("FD closed");		
	}
	vk_dbg("end of response handler");

	vk_end();
}

#include "vk_service.h"

void http11_request(struct vk_thread *that) {
	int rc = 0;
	int i;

	struct {
		struct vk_service service; /* via vk_copy_arg() */
		struct vk_rfcchunk chunk;
		ssize_t to_receive;
		int rc;
		char line[1024];
		char *tok;
		char *key;
		char *val;
		struct vk_future return_ft;
		struct request request;
		void *response;
		struct vk_thread *response_vk_ptr;
	} *self;

	vk_begin();

	vk_dbgf("http11_request() from client %s:%s to server %s:%s\n", vk_accepted_get_address_str(&self->service.accepted), vk_accepted_get_port_str(&self->service.accepted), vk_server_get_address_str(&self->service.server), vk_server_get_port_str(&self->service.server));
	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());

	vk_child(self->response_vk_ptr, http11_response);

	vk_request(self->response_vk_ptr, &self->return_ft, &self->request, self->response);
	if (self->response != 0) {
		vk_error();
	}

	do {
	    vk_dbg("start of request");

		/* request line */
		vk_readrfcline(rc, self->line, sizeof (self->line) - 1);
		self->line[rc] = '\0';
		if (rc == 0) {
			vk_raise_at(self->response_vk_ptr, EINVAL);
			vk_raise(EINVAL);
		} else if (rc == -1) {
			vk_error_at(self->response_vk_ptr);
			vk_error();
		}
		vk_dbgf("Request Line: %s\n", self->line);

		/* request method */
		self->request.method = NO_METHOD;
		self->tok = self->line;
		if (self->tok == NULL) {
			vk_raise_at(self->response_vk_ptr, EINVAL);
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
			vk_raise_at(self->response_vk_ptr, EINVAL);
			vk_raise(EINVAL);
		}
		self->val = strsep(&self->tok, " \t");
		copy_into(self->request.uri, self->val);

		/* request version */
		self->request.version = HTTP09;
		if (self->tok != NULL) {
			self->val = strsep(&self->tok, " \t");
			for (i = 0; i < VERSION_COUNT; i++) {
				if (strcasecmp(self->val, versions[i].repr) == 0) {
					self->request.version = versions[i].version;
					break;
				}
			}
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
			vk_readrfcheader(rc, self->line, sizeof (self->line) - 1, &self->key, &self->val);
			if (rc == 0) {
				break;
			} else if (rc == -1) {
				vk_error_at(self->response_vk_ptr);
				vk_error();
			}
			vk_dbgf("Header: %s: %s\n", self->key, self->val);

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
							vk_dbgf("content-length: %zu\n", self->request.content_length);
							break;
						case CONNECTION:
							if (strcasecmp(self->val, "close") == 0) {
								self->request.close = 1;
								vk_dbgf("%s\n", "closing connection");
							}
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

		if (strncmp(self->request.uri, "/crash", strlen("/crash")) == 0) {
			rc = 0;
        	rc = 5 / rc; /* trigger SIGILL */
		}

		vk_request(self->response_vk_ptr, &self->return_ft, &self->request, self->response);

		/* request entity */
		if (self->request.content_length > 0) {
			/* fixed size */

			vk_dbgf("Fixed entity of size %zu:\n", self->request.content_length);
			vk_clear();
			vk_write_splice(rc, self->request.content_length);
			if (rc == -1) {
                vk_error_at(self->response_vk_ptr);
                vk_error();
            }
			vk_hup();
		} else if (self->request.chunked) {
			/* chunked */
			vk_dbgf("%s", "Chunked entity:\n");

            vk_clear();
			do {
				vk_readrfcchunk_proto(rc, &self->chunk);
				if (rc == 0) {
					vk_dbgf("%s", "End of chunks.\n");
					break;
				}
				vk_dbgf("Chunk of size %zu:\n%.*s", self->chunk.size, (int) self->chunk.size, self->chunk.buf);
				vk_writerfcchunk(&self->chunk);
			} while (self->chunk.size > 0);
			vk_hup();

			/* request trailers */
			for (;;) {
				vk_readrfcheader(rc, self->line, sizeof (self->line) - 1, &self->key, &self->val);
				if (rc == 0) {
					break;
				} else if (rc == -1) {
					vk_error_at(self->response_vk_ptr);
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
			    /* response is waiting on vk_listen() */
			    vk_raise_at(self->response_vk_ptr, EINVAL);
			    vk_raise(EINVAL);
			    /*
				vk_raise_at(self->response_vk_ptr, EINVAL);
				vk_raise(EINVAL);
				*/
			}

			vk_readline(rc, self->line, sizeof (self->line) - 1);
			if (rc != 0) {
				vk_error_at(self->response_vk_ptr);
				vk_error();
			}
			rtrim(self->line, &rc);
			self->line[rc] = '\0';

			if (rc != 0) {
				vk_raise_at(self->response_vk_ptr, EINVAL);
				vk_raise(EINVAL);
			}
		} else {
			/* no entity, not chunked */
			vk_dbg("no entity");
			vk_hup();
			vk_dbg("clear for next request");
		}

		vk_dbg("end of request");
		if (self->request.close) {
			vk_dbg("closing connection");
		}
	} while (!vk_nodata() && !self->request.close);

	vk_play(self->response_vk_ptr);
    vk_dbg("closing FD");
    vk_tx_close();
    vk_dbg("FD closed");

	errno = 0;
	vk_finally();
	if (errno != 0) {
		if (errno == EFAULT && vk_get_signal() != 0) {
			vk_raise_at(self->response_vk_ptr, EFAULT);
			vk_play(self->response_vk_ptr);
			vk_raise(EINVAL);
		} else {
			vk_perror("request error");
		}
	}

	vk_free(); // self->response_vk_ptr
	vk_dbg("end of request handler");
	vk_end();
}

#include "vk_server.h"
#include <netinet/in.h>

int main(int argc, char *argv[]) {
	int rc;
	struct vk_server *server_ptr;
	struct vk_pool *pool_ptr;
	struct sockaddr_in address;

	server_ptr = calloc(1, vk_server_alloc_size());
	pool_ptr = calloc(1, vk_pool_alloc_size());

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	vk_server_set_pool(server_ptr, pool_ptr);
	vk_server_set_socket(server_ptr, PF_INET, SOCK_STREAM, 0);
	vk_server_set_address(server_ptr, (struct sockaddr *) &address, sizeof (address));
	vk_server_set_backlog(server_ptr, 128);
	vk_server_set_vk_func(server_ptr, http11_request);
	vk_server_set_count(server_ptr, 0);
	vk_server_set_page_count(server_ptr, 26);
	vk_server_set_msg(server_ptr, NULL);
	rc = vk_server_init(server_ptr);
	if (rc == -1) {
		return 1;
	}

	return 0;
}
