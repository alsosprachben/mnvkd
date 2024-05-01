#include "vk_thread.h"
#include "vk_accepted_s.h"
#include "vk_fetch.h"
#include "vk_debug.h"
#include "vk_future_s.h" /* for `struct vk_future` */
#include "vk_fetch_s.h" /* Include the fetch structure definition */
#include "vk_rfc.h"
#include "vk_rfc_s.h"

#include "vk_enum.h"

DEFINE_ENUM_FUNCTIONS(HTTP_METHOD, methods, method, repr, http_method);
DEFINE_ENUM_FUNCTIONS(HTTP_VERSION, versions, version, repr, http_version);
DEFINE_ENUM_FUNCTIONS(HTTP_HEADER, http_headers, http_header, repr, http_header);
DEFINE_ENUM_FUNCTIONS(HTTP_TRAILER, http_trailers, http_trailer, repr, http_trailer);

void vk_fetch_request(struct vk_thread* that)
{
	int rc = 0;

	struct {

		struct vk_fetch fetch;
		struct vk_accepted accepted;
		char fmt_buf[1024];
		int i;
		struct vk_rfcchunk chunk;
		struct vk_future request_ft;
		struct vk_future *response_ft_ptr;
		void *response;
		struct vk_thread* response_vk_ptr;
	} *self;

	vk_begin();

	vk_free(); /* to reallocate vk_fetch with headers */
	vk_calloc_size(self, 1, sizeof (*self) + sizeof (struct vk_header) * 32);

	vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());

	vk_spawn(self->response_vk_ptr, vk_fetch_response, &self->request_ft, &self->fetch, self->response_ft_ptr, self->response);

	/* Loop over requests until the connection should be closed */
	do {
		/* get fetch request */
		vk_read(rc, (char*)&self->fetch, sizeof(self->fetch));
		if (rc != sizeof(self->fetch)) {
			errno = EPIPE;
			vk_error();
		}

		/* Construct the HTTP request line */
		vk_writef(rc, self->fmt_buf, sizeof(self->fmt_buf), "%s %s HTTP/1.1\r\n", self->fetch.method,
			  self->fetch.url);

		/* Construct the HTTP headers */
		for (self->i = 0; self->i < self->fetch.headers.header_count; self->i++) {
			vk_writerfcheader(self->fetch.headers.headers[self->i].key, strlen(self->fetch.headers.headers[self->i].key),
					  self->fetch.headers.headers[self->i].value,
					  strlen(self->fetch.headers.headers[self->i].value));
		}
		/* Add an extra newline to separate headers from body */
		vk_writerfcheaderend();

		while (!vk_pollhup()) {
			vk_readrfcchunk(rc, &self->chunk);
			vk_dbgf("chunk.size = %zu: %.*s\n", self->chunk.size, (int)self->chunk.size, self->chunk.buf);
			vk_writerfcchunk_proto(rc, &self->chunk);
		}
		vk_readhup();
	} while (!self->fetch.close);


	errno = 0;
	vk_finally();
	if (errno != 0) {
		if (errno == EFAULT && vk_get_signal() != 0) {
			vk_raise_at(self->response_vk_ptr, EFAULT);
			vk_play(self->response_vk_ptr);
		} else {
			vk_perror("request error");
			vk_raise_at(self->response_vk_ptr, errno);
			vk_play(self->response_vk_ptr);
		}
	}

	vk_dbg("end of request handler");

	vk_end();
}

void vk_fetch_response(struct vk_thread* that)
{

	ssize_t rc = 0;

	struct {
		struct vk_accepted* accepted_ptr; /* via vk_fetch_request via vk_copy_arg() */
		struct vk_rfcchunk chunk;
		struct vk_future* parent_ft_ptr;
		struct vk_future child_ft;
		struct vk_fetch fetch;
		int error_cycle;
	}* self;

	vk_begin_pipeline(self->parent_ft_ptr, &self->child_ft);
	self->accepted_ptr = vk_future_get(self->parent_ft_ptr);

	do {

	} while (!self->fetch.close);

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
					rc = vk_snfault(errline, sizeof(errline) - 1);
					if (rc == -1) {
						/* This is safe because it will not lead back to an EFAULT error, so it
						 * is not recursive. */
						vk_error();
					}
					vk_clear_signal();
					rc = snprintf(self->chunk.buf, sizeof(self->chunk.buf) - 1,
						      "{\n"
						      "\"error\": \"%s\",\n"
						      "}\n",
						      errline);
					vk_log(errline);
				} else {
					/* regular errno error */
					rc = snprintf(self->chunk.buf, sizeof(self->chunk.buf) - 1,
					              "{\n"
					              "\"error\": \"%s\",\n"
					              "}\n",
					              strerror(errno));
				}
			}
			if (rc == -1) {
				vk_error();
			}
			vk_write(self->chunk.buf, rc);
			vk_flush();
			vk_tx_close();
		}
	}

	vk_free(); /* deallocation to pair with the allocation of this child thread */
	vk_dbg("end of response handler");

	vk_end();

}
