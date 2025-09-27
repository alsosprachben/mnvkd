#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <liburing.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static int set_nonblock(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl == -1) return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static int await_cqe(struct io_uring *ring, const char *fallback)
{
    struct io_uring_cqe *cqe = NULL;
    struct __kernel_timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
    int rc = io_uring_wait_cqe_timeout(ring, &cqe, &ts);
    if (rc == -ETIME) {
        printf("io_uring_wait_cqe(%s): no completion within timeout\n", fallback);
        return rc;
    }
    if (rc < 0) {
        printf("io_uring_wait_cqe(%s) rc=%d (%s)\n", fallback, rc, strerror(-rc));
        return rc;
    }

    const char *label = io_uring_cqe_get_data(cqe);
    if (!label) label = fallback;
    printf("%s: cqe->res=%d cqe->flags=0x%x\n", label, cqe->res, cqe->flags);
    io_uring_cqe_seen(ring, cqe);
    return cqe->res;
}

static int submit_poll(struct io_uring *ring, int fd, int events, const char *label)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printf("io_uring_get_sqe(%s) returned NULL\n", label);
        return -ENOMEM;
    }
    io_uring_prep_poll_add(sqe, fd, events);
    io_uring_sqe_set_data(sqe, (void *)label);

    int rc = io_uring_submit(ring);
    if (rc < 0) {
        printf("io_uring_submit(%s) rc=%d (%s)\n", label, rc, strerror(-rc));
        return rc;
    }
    printf("io_uring_submit(%s events=0x%x) rc=%d (submitted)\n", label, events, rc);
    return await_cqe(ring, label);
}

static int submit_recv(struct io_uring *ring, int fd, void *buf, size_t len, const char *label)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printf("io_uring_get_sqe(%s) returned NULL\n", label);
        return -ENOMEM;
    }
    io_uring_prep_recv(sqe, fd, buf, len, 0);
    io_uring_sqe_set_data(sqe, (void *)label);

    int rc = io_uring_submit(ring);
    if (rc < 0) {
        printf("io_uring_submit(%s) rc=%d (%s)\n", label, rc, strerror(-rc));
        return rc;
    }
    printf("io_uring_submit(%s len=%zu) rc=%d (submitted)\n", label, len, rc);
    return await_cqe(ring, label);
}

static int submit_send(struct io_uring *ring, int fd, const void *buf, size_t len, const char *label)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printf("io_uring_get_sqe(%s) returned NULL\n", label);
        return -ENOMEM;
    }
    io_uring_prep_send(sqe, fd, buf, len, 0);
    io_uring_sqe_set_data(sqe, (void *)label);

    int rc = io_uring_submit(ring);
    if (rc < 0) {
        printf("io_uring_submit(%s) rc=%d (%s)\n", label, rc, strerror(-rc));
        return rc;
    }
    printf("io_uring_submit(%s len=%zu) rc=%d (submitted)\n", label, len, rc);
    return await_cqe(ring, label);
}

int main(void)
{
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        perror("socketpair");
        return 1;
    }
    set_nonblock(sv[0]);
    set_nonblock(sv[1]);

    struct io_uring ring;
    int rc = io_uring_queue_init(32, &ring, 0);
    if (rc < 0) {
        printf("io_uring_queue_init rc=%d (%s)\n", rc, strerror(-rc));
        close(sv[0]);
        close(sv[1]);
        return 1;
    }

    char rbuf[64];
    const char *wmsg = "hello-from-io_uring";

    printf("\n=== Case 1: POLL for readability with NO data (not-ready) ===\n");
    submit_poll(&ring, sv[0], POLLIN, "POLL(no data)");

    printf("\n=== Case 2: RECV on socket with NO data (not-ready) ===\n");
    memset(rbuf, 0, sizeof(rbuf));
    submit_recv(&ring, sv[0], rbuf, sizeof(rbuf), "RECV(no data)");

    printf("\n=== Case 3: Make data available; POLL+RECV on socket (ready) ===\n");
    (void)write(sv[1], "abc", 3);
    submit_poll(&ring, sv[0], POLLIN, "POLL(data ready)");
    memset(rbuf, 0, sizeof(rbuf));
    submit_recv(&ring, sv[0], rbuf, sizeof(rbuf), "RECV(data ready)");
    (void)read(sv[0], rbuf, sizeof(rbuf));

    printf("\n=== Case 4: SEND small message (likely writable) ===\n");
    submit_send(&ring, sv[0], wmsg, strlen(wmsg), "SEND(small)");
    (void)read(sv[1], rbuf, sizeof(rbuf));

    printf("\n=== Case 5: Try to make send buffer tight; then SEND ===\n");
    char big[8192];
    memset(big, 'X', sizeof(big));
    size_t pushed = 0;
    for (;;) {
        ssize_t n = write(sv[0], big, sizeof(big));
        if (n > 0) { pushed += (size_t)n; continue; }
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        break;
    }
    printf("filled approx %zu bytes into send buffer\n", pushed);
    submit_send(&ring, sv[0], wmsg, strlen(wmsg), "SEND(tight)");

    io_uring_queue_exit(&ring);
    close(sv[0]);
    close(sv[1]);
    return 0;
}
