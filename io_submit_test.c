#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <libaio.h>
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

static void dump_event(const char *label, struct io_event *ev)
{
    printf("%s: cqe.res=%ld cqe.res2=%ld obj=%p\n", label, (long)ev->res, (long)ev->res2, ev->obj);
}

static int submit_pread(io_context_t ctx, int fd, void *buf, size_t len)
{
    struct iocb cb;
    struct iocb *cbs[1];
    memset(&cb, 0, sizeof(cb));
    io_prep_pread(&cb, fd, buf, len, 0 /*offset*/);
    cbs[0] = &cb;
    int rc = io_submit(ctx, 1, cbs);
    if (rc < 0) {
        printf("io_submit(PREAD) rc=%d errno=%d (%s)\n", rc, errno, strerror(errno));
        return rc;
    }
    printf("io_submit(PREAD) rc=%d (submitted)\n", rc);

    struct io_event ev;
    memset(&ev, 0, sizeof(ev));
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 }; /* 1ms */
    rc = io_getevents(ctx, 1, 1, &ev, &ts);
    if (rc < 0) {
        printf("io_getevents after PREAD rc=%d errno=%d (%s)\n", rc, errno, strerror(errno));
    } else if (rc == 0) {
        printf("io_getevents after PREAD: no completion within timeout\n");
    } else {
        dump_event("PREAD", &ev);
    }
    return rc;
}

static int submit_pwrite(io_context_t ctx, int fd, const void *buf, size_t len)
{
    struct iocb cb;
    struct iocb *cbs[1];
    memset(&cb, 0, sizeof(cb));
    io_prep_pwrite(&cb, fd, (void *)buf, len, 0 /*offset*/);
    cbs[0] = &cb;
    int rc = io_submit(ctx, 1, cbs);
    if (rc < 0) {
        printf("io_submit(PWRITE) rc=%d errno=%d (%s)\n", rc, errno, strerror(errno));
        return rc;
    }
    printf("io_submit(PWRITE) rc=%d (submitted)\n", rc);

    struct io_event ev;
    memset(&ev, 0, sizeof(ev));
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 }; /* 1ms */
    rc = io_getevents(ctx, 1, 1, &ev, &ts);
    if (rc < 0) {
        printf("io_getevents after PWRITE rc=%d errno=%d (%s)\n", rc, errno, strerror(errno));
    } else if (rc == 0) {
        printf("io_getevents after PWRITE: no completion within timeout\n");
    } else {
        dump_event("PWRITE", &ev);
    }
    return rc;
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

    io_context_t ctx = 0; /* must be zeroed */
    int rc = io_setup(32, &ctx);
    if (rc < 0) {
        printf("io_setup rc=%d errno=%d (%s)\n", rc, errno, strerror(errno));
        close(sv[0]);
        close(sv[1]);
        return 1;
    }

    char rbuf[64];
    const char *wmsg = "hello-from-aio";

    printf("\n=== Case 1: PREAD on socket with NO data (not-ready) ===\n");
    submit_pread(ctx, sv[0], rbuf, sizeof(rbuf));

    printf("\n=== Case 2: Make data available; PREAD on socket (ready) ===\n");
    (void)write(sv[1], "abc", 3);
    submit_pread(ctx, sv[0], rbuf, sizeof(rbuf));
    /* drain */
    (void)read(sv[0], rbuf, sizeof(rbuf));

    printf("\n=== Case 3: PWRITE small message (likely writable) ===\n");
    submit_pwrite(ctx, sv[0], wmsg, strlen(wmsg));
    /* drain peer */
    (void)read(sv[1], rbuf, sizeof(rbuf));

    printf("\n=== Case 4: Try to make send buffer tight; then PWRITE ===\n");
    /* best-effort: fill sv[0] send buffer */
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
    submit_pwrite(ctx, sv[0], wmsg, strlen(wmsg));

    io_destroy(ctx);
    close(sv[0]);
    close(sv[1]);
    return 0;
}

