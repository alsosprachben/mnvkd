#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "vk_thread_cr.h"
#include "vk_thread_exec.h"
#include "vk_thread_mem.h"
#include "vk_thread_io.h"
#include "vk_thread.h"
#include "vk_thread_s.h"
#include "vk_proc_local.h"
#include "vk_proc_local_s.h"
#include "vk_accepted.h"
#include "vk_accepted_s.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "vk_stack.h"
#include "vk_pipe.h"
#include "vk_io_queue.h"
#include "vk_io_queue_s.h"

enum test_mode {
    TEST_MODE_READ = 0,
    TEST_MODE_WRITE,
    TEST_MODE_FLUSH,
    TEST_MODE_TX_CLOSE,
    TEST_MODE_RX_CLOSE,
    TEST_MODE_TX_SHUTDOWN,
    TEST_MODE_RX_SHUTDOWN,
    TEST_MODE_ACCEPT,
};

struct test_self {
    struct vk_io_op* pending;
    struct vk_io_queue* queue;
    enum test_mode mode;
    char buf[256];
    size_t data_len;
    ssize_t bytes;
    ssize_t last_res;
    int shutdown_how;
    int accepted_fd;
};

static struct vk_io_queue* g_queue_ptr;
static enum test_mode g_queue_mode;
static const char* g_queue_data;
static size_t g_queue_data_len;
static int g_queue_shutdown_how;

#define STACK_BYTES (1 << 18)

struct test_context {
    struct vk_proc_local proc_local;
    char stack_mem[STACK_BYTES];
    struct vk_pipe rx_pipe;
    struct vk_pipe tx_pipe;
    struct vk_thread thread;
    struct vk_io_queue queue;
};

struct accept_ctx {
    int listener;
    int* accepted_fd_ptr;
};

static void init_context(struct test_context* ctx,
                         int fd_rx,
                         enum vk_fd_type rx_type,
                         int fd_tx,
                         enum vk_fd_type tx_type,
                         enum test_mode mode,
                         const char* data,
                         size_t data_len,
                         int shutdown_how);
static int wait_for_event(struct vk_thread* thread,
                          struct test_self* self,
                          int (*simulate)(struct vk_io_op*, struct test_self*, void*),
                          void* userdata);
static int simulate_read(struct vk_io_op* op, struct test_self* self, void* userdata);
static int simulate_write(struct vk_io_op* op, struct test_self* self, void* userdata);
static int simulate_close(struct vk_io_op* op, struct test_self* self, void* userdata);
static int simulate_shutdown(struct vk_io_op* op, struct test_self* self, void* userdata);
static int simulate_accept(struct vk_io_op* op, struct test_self* self, void* userdata);
static int test_read_case(void);
static int test_write_case(void);
static int test_flush_case(void);
static int test_tx_close_case(void);
static int test_rx_close_case(void);
static int test_tx_shutdown_case(void);
static int test_rx_shutdown_case(void);
static int test_accept_case(void);

#define VK_TEST_QUEUE_OP(self_ptr, socket_ptr, queue_ptr)                                                           \
    do {                                                                                                           \
        (self_ptr)->pending = vk_thread_queue_io_op(that, (socket_ptr), (queue_ptr));                             \
        if ((self_ptr)->pending == NULL) {                                                                        \
            vk_error();                                                                                           \
        }                                                                                                         \
        VK_IO_QUEUE_WAIT(that, (socket_ptr));                                                                     \
        vk_thread_io_complete_op((socket_ptr), (queue_ptr), (self_ptr)->pending);                                 \
        (self_ptr)->last_res = (self_ptr)->pending ? (self_ptr)->pending->res : 0;                                \
        (self_ptr)->pending = NULL;                                                                               \
    } while (0)

static void io_queue_coroutine(struct vk_thread* that)
{
    struct vk_socket* sock = vk_get_socket(that);
    struct test_self* self;

    vk_begin();

    self->pending = NULL;
    self->queue = g_queue_ptr;
    self->mode = g_queue_mode;
    self->data_len = g_queue_data_len;
    self->shutdown_how = g_queue_shutdown_how;
    self->bytes = 0;
    self->last_res = 0;
    self->accepted_fd = -1;

    if (g_queue_data && g_queue_data_len > 0 && g_queue_data_len <= sizeof(self->buf)) {
        memcpy(self->buf, g_queue_data, g_queue_data_len);
    }

    fprintf(stderr, "coroutine mode=%d\n", self->mode);

    if (self->mode == TEST_MODE_READ || self->mode == TEST_MODE_ACCEPT) {
        vk_block_init(vk_socket_get_block(sock), self->buf, sizeof(self->buf), VK_OP_READ);
        VK_TEST_QUEUE_OP(self, sock, self->queue);
        self->bytes = self->last_res;
    } else if (self->mode == TEST_MODE_WRITE) {
        vk_block_init(vk_socket_get_block(sock), NULL, 0, VK_OP_WRITE);
        VK_TEST_QUEUE_OP(self, sock, self->queue);
        self->bytes = self->last_res;
    } else if (self->mode == TEST_MODE_FLUSH) {
        vk_block_init(vk_socket_get_block(sock), NULL, 0, VK_OP_FLUSH);
        VK_TEST_QUEUE_OP(self, sock, self->queue);
        self->bytes = self->last_res;
    } else if (self->mode == TEST_MODE_TX_CLOSE) {
        vk_block_init(vk_socket_get_block(sock), NULL, 0, VK_OP_TX_CLOSE);
        VK_TEST_QUEUE_OP(self, sock, self->queue);
    } else if (self->mode == TEST_MODE_RX_CLOSE) {
        vk_block_init(vk_socket_get_block(sock), NULL, 0, VK_OP_RX_CLOSE);
        VK_TEST_QUEUE_OP(self, sock, self->queue);
    } else if (self->mode == TEST_MODE_TX_SHUTDOWN) {
        vk_block_init(vk_socket_get_block(sock), NULL, 0, VK_OP_TX_SHUTDOWN);
        VK_TEST_QUEUE_OP(self, sock, self->queue);
    } else if (self->mode == TEST_MODE_RX_SHUTDOWN) {
        vk_block_init(vk_socket_get_block(sock), NULL, 0, VK_OP_RX_SHUTDOWN);
        VK_TEST_QUEUE_OP(self, sock, self->queue);
    } else {
        vk_error();
    }

    vk_end();
}

static int test_read_case(void);
static int test_write_case(void);
static int test_flush_case(void);
static int test_tx_close_case(void);
static int test_rx_close_case(void);
static int test_tx_shutdown_case(void);
static int test_rx_shutdown_case(void);
static int test_accept_case(void);

static int run_io_queue_test(void)
{
    if (test_read_case() != 0) return 1;
    if (test_write_case() != 0) return 1;
    if (test_flush_case() != 0) return 1;
    if (test_tx_close_case() != 0) return 1;
    if (test_rx_close_case() != 0) return 1;
    if (test_tx_shutdown_case() != 0) return 1;
    if (test_rx_shutdown_case() != 0) return 1;
    if (test_accept_case() != 0) return 1;
    return 0;
}

static int test_read_case(void)
{
    const char payload[] = "hello";
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    if (write(fds[1], payload, sizeof(payload)) != (ssize_t)sizeof(payload)) {
        perror("write");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 fds[0], VK_FD_TYPE_SOCKET_STREAM,
                 fds[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_READ,
                 payload, sizeof(payload),
                 0);

    struct test_self* self = vk_get_self(&ctx.thread);
    if (wait_for_event(&ctx.thread, self, simulate_read, NULL) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (self->bytes != (ssize_t)sizeof(payload)) {
        fprintf(stderr, "unexpected byte count %zd\n", self->bytes);
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    char out[sizeof(payload)];
    ssize_t pulled = vk_vectoring_recv(&ctx.thread.socket_ptr->rx.ring, out, sizeof(out));
    if (pulled != (ssize_t)sizeof(payload) || memcmp(out, payload, sizeof(payload)) != 0) {
        fprintf(stderr, "ring contents mismatch\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    close(fds[0]);
    close(fds[1]);
    g_queue_ptr = NULL;
    return 0;
}

static int test_write_case(void)
{
    const char payload[] = "world";
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 fds[0], VK_FD_TYPE_SOCKET_STREAM,
                 fds[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_WRITE,
                 payload, sizeof(payload),
                 0);

    struct vk_socket* sock = ctx.thread.socket_ptr;
    if (vk_vectoring_send(&sock->tx.ring, payload, sizeof(payload)) != (ssize_t)sizeof(payload)) {
        fprintf(stderr, "failed to seed tx ring\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    struct test_self* self = vk_get_self(&ctx.thread);
    if (wait_for_event(&ctx.thread, self, simulate_write, NULL) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    char out[sizeof(payload)];
    ssize_t read_bytes = read(fds[0], out, sizeof(out));
    if (read_bytes != (ssize_t)sizeof(payload) || memcmp(out, payload, sizeof(payload)) != 0) {
        fprintf(stderr, "written payload mismatch\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    close(fds[0]);
    close(fds[1]);
    g_queue_ptr = NULL;
    return 0;
}

static int test_flush_case(void)
{
    const char payload[] = "flush";
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 fds[0], VK_FD_TYPE_SOCKET_STREAM,
                 fds[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_FLUSH,
                 payload, sizeof(payload),
                 0);

    struct vk_socket* sock = ctx.thread.socket_ptr;
    if (vk_vectoring_send(&sock->tx.ring, payload, sizeof(payload)) != (ssize_t)sizeof(payload)) {
        fprintf(stderr, "failed to seed tx ring for flush\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    struct test_self* self = vk_get_self(&ctx.thread);
    if (wait_for_event(&ctx.thread, self, simulate_write, NULL) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    char out[sizeof(payload)];
    ssize_t read_bytes = read(fds[0], out, sizeof(out));
    if (read_bytes != (ssize_t)sizeof(payload) || memcmp(out, payload, sizeof(payload)) != 0) {
        fprintf(stderr, "flush payload mismatch\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    close(fds[0]);
    close(fds[1]);
    g_queue_ptr = NULL;
    return 0;
}

static int test_tx_close_case(void)
{
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 fds[0], VK_FD_TYPE_SOCKET_STREAM,
                 fds[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_TX_CLOSE,
                 NULL, 0,
                 0);

    struct test_self* self = vk_get_self(&ctx.thread);
    if (wait_for_event(&ctx.thread, self, simulate_close, NULL) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (!vk_vectoring_is_closed(&ctx.thread.socket_ptr->tx.ring) ||
        !vk_pipe_get_closed(&ctx.thread.socket_ptr->tx_fd)) {
        fprintf(stderr, "tx close not reflected in state\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    fds[1] = -1;
    if (fds[0] >= 0) close(fds[0]);
    if (fds[1] >= 0) close(fds[1]);
    g_queue_ptr = NULL;
    return 0;
}

static int test_rx_close_case(void)
{
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 fds[0], VK_FD_TYPE_SOCKET_STREAM,
                 fds[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_RX_CLOSE,
                 NULL, 0,
                 0);

    struct test_self* self = vk_get_self(&ctx.thread);
    if (wait_for_event(&ctx.thread, self, simulate_close, NULL) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (!vk_vectoring_is_closed(&ctx.thread.socket_ptr->rx.ring) ||
        !vk_pipe_get_closed(&ctx.thread.socket_ptr->rx_fd)) {
        fprintf(stderr, "rx close not reflected in state\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    fds[0] = -1;
    if (fds[0] >= 0) close(fds[0]);
    if (fds[1] >= 0) close(fds[1]);
    g_queue_ptr = NULL;
    return 0;
}

static int test_tx_shutdown_case(void)
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
        perror("socketpair");
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 fds[0], VK_FD_TYPE_SOCKET_STREAM,
                 fds[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_TX_SHUTDOWN,
                 NULL, 0,
                 SHUT_WR);

    struct test_self* self = vk_get_self(&ctx.thread);
    if (wait_for_event(&ctx.thread, self, simulate_shutdown, NULL) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (!vk_vectoring_is_closed(&ctx.thread.socket_ptr->tx.ring) ||
        !vk_pipe_get_closed(&ctx.thread.socket_ptr->tx_fd)) {
        fprintf(stderr, "tx shutdown not reflected in state\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (fds[0] >= 0) close(fds[0]);
    if (fds[1] >= 0) close(fds[1]);
    g_queue_ptr = NULL;
    return 0;
}

static int test_rx_shutdown_case(void)
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
        perror("socketpair");
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 fds[0], VK_FD_TYPE_SOCKET_STREAM,
                 fds[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_RX_SHUTDOWN,
                 NULL, 0,
                 SHUT_RD);

    struct test_self* self = vk_get_self(&ctx.thread);
    if (wait_for_event(&ctx.thread, self, simulate_shutdown, NULL) != 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (!vk_vectoring_is_closed(&ctx.thread.socket_ptr->rx.ring) ||
        !vk_pipe_get_closed(&ctx.thread.socket_ptr->rx_fd)) {
        fprintf(stderr, "rx shutdown not reflected in state\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    if (fds[0] >= 0) close(fds[0]);
    if (fds[1] >= 0) close(fds[1]);
    g_queue_ptr = NULL;
    return 0;
}

static int test_accept_case(void)
{
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("socket listener");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listener);
        return 1;
    }
    socklen_t addr_len = sizeof(addr);
    if (getsockname(listener, (struct sockaddr*)&addr, &addr_len) == -1) {
        perror("getsockname");
        close(listener);
        return 1;
    }
    if (listen(listener, 1) == -1) {
        perror("listen");
        close(listener);
        return 1;
    }

    int client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == -1) {
        perror("socket client");
        close(listener);
        return 1;
    }

    if (connect(client, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(client);
        close(listener);
        return 1;
    }

    int dummy[2];
    if (pipe(dummy) == -1) {
        perror("pipe dummy");
        close(client);
        close(listener);
        return 1;
    }

    struct test_context ctx;
    init_context(&ctx,
                 listener, VK_FD_TYPE_SOCKET_LISTEN,
                 dummy[1], VK_FD_TYPE_SOCKET_STREAM,
                 TEST_MODE_ACCEPT,
                 NULL, 0,
                 0);

    struct test_self* self = vk_get_self(&ctx.thread);
    struct accept_ctx actx = { listener, &self->accepted_fd };

    if (wait_for_event(&ctx.thread, self, simulate_accept, &actx) != 0) {
        close(dummy[0]);
        close(dummy[1]);
        close(client);
        close(listener);
        if (self->accepted_fd >= 0) close(self->accepted_fd);
        return 1;
    }

    if (self->bytes != (ssize_t)vk_accepted_alloc_size()) {
        fprintf(stderr, "unexpected accept bytes %zd\n", self->bytes);
        close(dummy[0]);
        close(dummy[1]);
        close(client);
        close(listener);
        if (self->accepted_fd >= 0) close(self->accepted_fd);
        return 1;
    }

    struct vk_accepted accepted;
    ssize_t pulled = vk_vectoring_recv(&ctx.thread.socket_ptr->rx.ring,
                                       &accepted,
                                       vk_accepted_alloc_size());
    if (pulled != (ssize_t)vk_accepted_alloc_size()) {
        fprintf(stderr, "accept struct size mismatch\n");
        close(dummy[0]);
        close(dummy[1]);
        close(client);
        close(listener);
        if (self->accepted_fd >= 0) close(self->accepted_fd);
        return 1;
    }

    if (self->accepted_fd < 0 || vk_accepted_get_fd(&accepted) != self->accepted_fd) {
        fprintf(stderr, "accepted fd mismatch\n");
        close(dummy[0]);
        close(dummy[1]);
        close(client);
        close(listener);
        if (self->accepted_fd >= 0) close(self->accepted_fd);
    }

    close(dummy[0]);
    close(dummy[1]);
    close(client);
    if (self->accepted_fd >= 0) close(self->accepted_fd);
    close(listener);
    g_queue_ptr = NULL;
    return 0;
}
static void init_context(struct test_context* ctx,
                         int fd_rx,
                         enum vk_fd_type rx_type,
                         int fd_tx,
                         enum vk_fd_type tx_type,
                         enum test_mode mode,
                         const char* data,
                         size_t data_len,
                         int shutdown_how)
{
    vk_proc_local_init(&ctx->proc_local);
    vk_stack_init(&ctx->proc_local.stack, ctx->stack_mem, sizeof(ctx->stack_mem));
    vk_pipe_init_fd(&ctx->rx_pipe, fd_rx, rx_type);
    vk_pipe_init_fd(&ctx->tx_pipe, fd_tx, tx_type);
    vk_thread_clear(&ctx->thread);
    vk_init(&ctx->thread, &ctx->proc_local, io_queue_coroutine, &ctx->rx_pipe, &ctx->tx_pipe,
            "io_queue_coroutine", __FILE__, __LINE__);
    vk_io_queue_init(&ctx->queue);
    vk_socket_set_io_queue(ctx->thread.socket_ptr, &ctx->queue);

    g_queue_ptr = &ctx->queue;
    g_queue_mode = mode;
    g_queue_data = data;
    g_queue_data_len = data_len;
    g_queue_shutdown_how = shutdown_how;
}

static int wait_for_event(struct vk_thread* thread,
                          struct test_self* self,
                          int (*simulate)(struct vk_io_op*, struct test_self*, void*),
                          void* userdata)
{
    while (vk_get_status(thread) != VK_PROC_END) {
        thread->func(thread);
        enum VK_PROC_STAT st = vk_get_status(thread);
        if (st == VK_PROC_WAIT || st == VK_PROC_DEFER) {
            struct vk_io_op* op = self->pending;
            if (!op) {
                fprintf(stderr, "pending op missing\n");
                return 1;
            }
            if (simulate(op, self, userdata) != 0) {
                return 1;
            }
            if (st == VK_PROC_DEFER) {
                vk_proc_local_drop_deferred(vk_get_proc_local(thread), thread);
            }
            vk_ready(thread);
        }
    }
    return 0;
}

struct simulate_rw_ctx {
    ssize_t (*syscall_fn)(int, const struct iovec*, int);
};

static int simulate_read(struct vk_io_op* op, struct test_self* self, void* userdata)
{
    (void)self;
    int iovcnt = op->iov[1].iov_len > 0 ? 2 : 1;
    ssize_t rc = readv(op->fd, op->iov, iovcnt);
    if (rc == -1) {
        perror("readv");
        return 1;
    }
    op->res = rc;
    op->err = 0;
    op->state = VK_IO_OP_DONE;
    return 0;
}

static int simulate_write(struct vk_io_op* op, struct test_self* self, void* userdata)
{
    (void)self;
    int iovcnt = op->iov[1].iov_len > 0 ? 2 : 1;
    ssize_t rc = writev(op->fd, op->iov, iovcnt);
    if (rc == -1) {
        perror("writev");
        return 1;
    }
    op->res = rc;
    op->err = 0;
    op->state = VK_IO_OP_DONE;
    return 0;
}

static int simulate_close(struct vk_io_op* op, struct test_self* self, void* userdata)
{
    (void)self;
    (void)userdata;
    if (close(op->fd) == -1) {
        perror("close");
        return 1;
    }
    op->fd = -1;
    op->res = 0;
    op->err = 0;
    op->state = VK_IO_OP_DONE;
    return 0;
}

static int simulate_shutdown(struct vk_io_op* op, struct test_self* self, void* userdata)
{
    (void)self;
    int how = (int)(intptr_t)op->tag2;
    fprintf(stderr, "simulate_shutdown fd=%d how=%d\n", op->fd, how);
    if (shutdown(op->fd, how) == -1) {
        perror("shutdown");
        return 1;
    }
    op->res = 0;
    op->err = 0;
    op->state = VK_IO_OP_DONE;
    return 0;
}

static int simulate_accept(struct vk_io_op* op, struct test_self* self, void* userdata)
{
    (void)self;
    struct accept_ctx* ctx = (struct accept_ctx*)userdata;
    struct vk_accepted accepted;
    int accepted_fd = vk_accepted_accept(&accepted, ctx->listener);
    if (accepted_fd == -1) {
        perror("accept");
        return 1;
    }

    size_t len = vk_accepted_alloc_size();
    const char* src = (const char*)&accepted;
    size_t offset = 0;
    for (int i = 0; i < 2 && offset < len; ++i) {
        size_t copy = op->iov[i].iov_len < (len - offset) ? op->iov[i].iov_len : (len - offset);
        memcpy(op->iov[i].iov_base, src + offset, copy);
        offset += copy;
    }

    op->res = (ssize_t)len;
    op->err = 0;
    op->state = VK_IO_OP_DONE;
    if (ctx->accepted_fd_ptr) {
        *ctx->accepted_fd_ptr = accepted_fd;
    }
    return 0;
}


int main(void)
{
    return run_io_queue_test();
}
