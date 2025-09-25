#include "vk_sys_caps.h"
#include "vk_sys_caps_s.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#if defined(__linux__)
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <linux/aio_abi.h>
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

    aio_context_t ctx = 0;
    long rc = syscall(SYS_io_setup, 16, &ctx);
    if (rc == 0) {
        caps->have_aio = 1;

        int pipes[2] = {-1, -1};
        if (pipe(pipes) == 0) {
            struct iocb cb;
            struct iocb* list[1];
            struct {
                uint64_t events;
                uint64_t resfd;
            } poll_data;
            memset(&cb, 0, sizeof(cb));
            poll_data.events = POLLIN;
            poll_data.resfd = 0;
            cb.aio_fildes = pipes[0];
            cb.aio_lio_opcode = IOCB_CMD_POLL;
            cb.aio_buf = (uint64_t)(uintptr_t)&poll_data;
            list[0] = &cb;

            errno = 0;
            rc = syscall(SYS_io_submit, ctx, 1, list);
            if (rc == 1) {
                caps->have_aio_poll = 1;
                struct io_event event;
                memset(&event, 0, sizeof(event));
                (void)syscall(SYS_io_cancel, ctx, &cb, &event);
            }
            close(pipes[0]);
            close(pipes[1]);
        }

        syscall(SYS_io_destroy, ctx);
    }
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
