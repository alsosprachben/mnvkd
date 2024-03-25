#include "vk_fetch.h"
#include "vk_fetch_s.h"  /* Include the fetch structure definition */
#include "vk_debug.h"
#include "vk_rfc.h"
#include "vk_rfc_s.h"

void vk_fetch_request(struct vk_thread *that) {
    int rc = 0;

    struct {
        struct vk_fetch fetch;
        char fmt_buf[1024];
        int i;
        struct vk_rfcchunk chunk;
    } *self;

    vk_begin();

    /* Loop over requests until the connection should be closed */
    while (!self->fetch.close) {
        /* Construct the HTTP request line */
        vk_writef(rc, self->fmt_buf, sizeof(self->fmt_buf), "%s %s HTTP/1.1\r\n", self->fetch.method, self->fetch.url);

        /* Construct the HTTP headers */
        for (self->i = 0; self->i < self->fetch.header_count; self->i++) {
            vk_writerfcheader(self->fetch.headers[self->i].key, strlen(self->fetch.headers[self->i].key), self->fetch.headers[self->i].value, strlen(self->fetch.headers[self->i].value));
        }
        /* Add an extra newline to separate headers from body */
        vk_writerfcheaderend();

        while (!vk_nodata()) {
            vk_readrfcchunk(rc, &self->chunk);
			vk_dbgf("chunk.size = %zu: %.*s\n", self->chunk.size, (int) self->chunk.size, self->chunk.buf);
            vk_writerfcchunk_proto(rc, &self->chunk);
        }
        vk_clear();

        /* If the connection should be closed after this request, close it */
        if (self->fetch.close) {
            vk_tx_close();
        }
    }

    vk_end();
}

void vk_fetch_response(struct vk_thread *that) {
    /* TODO: Implement the fetch response function */
}