#include <openssl/ssl.h>
#include "vk_main_local.h"
#include "vk_socket.h"

void tls_server(struct vk_thread *that);

void tls_client(struct vk_thread *that)
{
    struct {
        struct vk_thread* server_vk;
        SSL_CTX* ctx;
        int rc;
        char line[64];
    } *self;
    vk_begin();

    vk_calloc_size(self->server_vk, 1, vk_alloc_size());
    vk_responder(self->server_vk, tls_server);
    vk_play(self->server_vk);

    self->ctx = SSL_CTX_new(TLS_client_method());
    vk_socket_init_tls(vk_get_socket(that), self->ctx);

    vk_tls_connect(self->rc);

    vk_readline(self->rc, self->line, sizeof(self->line) - 1);
    vk_log(self->line);

    SSL_CTX_free(self->ctx);
    vk_free();

    vk_end();
}

void tls_server(struct vk_thread *that)
{
    struct {
        SSL_CTX* ctx;
        int rc;
    } *self;
    vk_begin();

    self->ctx = SSL_CTX_new(TLS_server_method());
    SSL_CTX_use_certificate_chain_file(self->ctx, "chain.pem");
    SSL_CTX_use_PrivateKey_file(self->ctx, "pkey.pem", SSL_FILETYPE_PEM);
    vk_socket_init_tls(vk_get_socket(that), self->ctx);

    vk_tls_accept(self->rc);

    vk_write_literal("hello\n");
    vk_flush();

    SSL_CTX_free(self->ctx);
    vk_end();
}

int main()
{
    return vk_main_local_init(tls_client, NULL, 0, 44);
}
