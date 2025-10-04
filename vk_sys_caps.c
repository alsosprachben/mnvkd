#include "vk_sys_caps.h"
#include "vk_sys_caps_s.h"
#include "vk_debug.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#if defined(__linux__)
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <libaio.h>
#include <linux/io_uring.h>

#ifndef SYS_io_uring_setup
#ifdef __NR_io_uring_setup
#define SYS_io_uring_setup __NR_io_uring_setup
#endif
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#endif

void
vk_sys_caps_init(struct vk_sys_caps* caps)
{
    if (!caps) {
        return;
    }
    memset(caps, 0, sizeof(*caps));
}

int
vk_sys_caps_is_probed(const struct vk_sys_caps* caps)
{
    return caps && caps->probed;
}

int
vk_sys_caps_have_epoll(const struct vk_sys_caps* caps)
{
    return caps && caps->have_epoll;
}

int
vk_sys_caps_have_kqueue(const struct vk_sys_caps* caps)
{
    return caps && caps->have_kqueue;
}

int
vk_sys_caps_have_aio(const struct vk_sys_caps* caps)
{
    return caps && caps->have_aio;
}

int
vk_sys_caps_have_aio_poll(const struct vk_sys_caps* caps)
{
    return caps && caps->have_aio_poll;
}

int
vk_sys_caps_have_io_uring(const struct vk_sys_caps* caps)
{
    return caps && caps->have_io_uring;
}

int
vk_sys_caps_have_io_uring_fast_poll(const struct vk_sys_caps* caps)
{
    return caps && caps->have_io_uring_fast_poll;
}

int
vk_sys_caps_detect(struct vk_sys_caps* caps)
{
    if (!caps) {
        errno = EINVAL;
        return -1;
    }

    vk_sys_caps_init(caps);

#if defined(__linux__)
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd >= 0) {
        caps->have_epoll = 1;
        close(epoll_fd);
    }

    io_context_t ctx = 0;
    long rc = syscall(SYS_io_setup, 16, &ctx);
    if (rc == 0) {
        caps->have_aio = 1;

        int sv[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0) {
            int fl0 = fcntl(sv[0], F_GETFL, 0);
            if (fl0 != -1) {
                (void)fcntl(sv[0], F_SETFL, fl0 | O_NONBLOCK);
            }
            int fl1 = fcntl(sv[1], F_GETFL, 0);
            if (fl1 != -1) {
                (void)fcntl(sv[1], F_SETFL, fl1 | O_NONBLOCK);
            }
            struct iocb cb;
            struct iocb* list[1];
            io_prep_poll(&cb, sv[0], POLLIN);
            list[0] = &cb;

            errno = 0;
            rc = syscall(SYS_io_submit, ctx, 1, list);
            DBG("sys_caps: io_submit(POLL) rc=%ld errno=%d\n", rc, errno);
            if (rc == 1) {
                caps->have_aio_poll = 1;
                DBG("sys_caps: have_aio_poll=1\n");
                struct io_event event;
                memset(&event, 0, sizeof(event));
                (void)syscall(SYS_io_cancel, ctx, &cb, &event);
            }
            close(sv[0]);
            close(sv[1]);
        }

        syscall(SYS_io_destroy, ctx);
    }

#if defined(__linux__) && defined(SYS_io_uring_setup)
    int allow_io_uring = 1;
    const char* disable_uring_env = getenv("VK_DISABLE_IO_URING");
    if (disable_uring_env && disable_uring_env[0] != '\0' && disable_uring_env[0] != '0') {
        allow_io_uring = 0;
        DBG("sys_caps: VK_DISABLE_IO_URING set; skipping io_uring detection\n");
    }
    if (allow_io_uring) {
        struct io_uring_params params;
        memset(&params, 0, sizeof(params));
        errno = 0;
        int ring_fd = syscall(SYS_io_uring_setup, 1, &params);
        DBG("sys_caps: io_uring_setup rc=%d errno=%d\n", ring_fd, errno);
        if (ring_fd >= 0) {
            caps->have_io_uring = 1;
            if (params.features & IORING_FEAT_FAST_POLL) {
                caps->have_io_uring_fast_poll = 1;
            }
            close(ring_fd);
        }
    }
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    int kq = kqueue();
    if (kq >= 0) {
        caps->have_kqueue = 1;
        close(kq);
    }
#endif

    caps->probed = 1;
    return 0;
}
