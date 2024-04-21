#include "vk_fetch.h"
#include "vk_debug.h"
#include "vk_fetch_s.h" /* Include the fetch structure definition */
#include "vk_rfc.h"
#include "vk_rfc_s.h"

void vk_fetch_request(struct vk_thread* that)
{
	int rc = 0;

	struct {
		struct vk_fetch fetch;
		char fmt_buf[1024];
		int i;
		struct vk_rfcchunk chunk;
	}* self;

	vk_begin();

	/* Loop over requests until the connection should be closed */
	do {
		/* Construct the HTTP request line */
		vk_writef(rc, self->fmt_buf, sizeof(self->fmt_buf), "%s %s HTTP/1.1\r\n", self->fetch.method,
			  self->fetch.url);

		/* Construct the HTTP headers */
		for (self->i = 0; self->i < self->fetch.header_count; self->i++) {
			vk_writerfcheader(self->fetch.headers[self->i].key, strlen(self->fetch.headers[self->i].key),
					  self->fetch.headers[self->i].value,
					  strlen(self->fetch.headers[self->i].value));
		}
		/* Add an extra newline to separate headers from body */
		vk_writerfcheaderend();

		while (!vk_nodata()) {
			vk_readrfcchunk(rc, &self->chunk);
			vk_dbgf("chunk.size = %zu: %.*s\n", self->chunk.size, (int)self->chunk.size, self->chunk.buf);
			vk_writerfcchunk_proto(rc, &self->chunk);
		}
		vk_clear();
	} while (!self->fetch.close);

	vk_end();
}

void vk_fetch_response(struct vk_thread* that)
{ /* TODO: Implement the fetch response function */
}

#include "vk_server.h"
#include <netinet/in.h>

int main(int argc, char* argv[])
{
	int rc;
	struct vk_server* server_ptr;
	struct vk_pool* pool_ptr;
	struct sockaddr_in address;

	server_ptr = calloc(1, vk_server_alloc_size());
	pool_ptr = calloc(1, vk_pool_alloc_size());

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_LOOPBACK;
	address.sin_port = htons(8080);

	vk_server_set_pool(server_ptr, pool_ptr);
	vk_server_set_socket(server_ptr, PF_INET, SOCK_STREAM, 0);
	vk_server_set_address(server_ptr, (struct sockaddr*)&address, sizeof(address));
	vk_server_set_backlog(server_ptr, 128);
	vk_server_set_vk_func(server_ptr, vk_fetch_request);
	vk_server_set_count(server_ptr, 0);
	vk_server_set_privileged(server_ptr, 0);
	vk_server_set_isolated(server_ptr, 1);
	vk_server_set_page_count(server_ptr, 26);
	vk_server_set_msg(server_ptr, NULL);
	rc = vk_server_init(server_ptr);
	if (rc == -1) {
		return 1;
	}

	return 0;
}
