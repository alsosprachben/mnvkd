#include <poll.h>
#include <string.h>

#include "vk_io_batch_sync.h"
#include "vk_io_queue.h"
#include "vk_io_queue_s.h"
#include "vk_io_op.h"
#include "vk_io_op_s.h"
#include "vk_io_exec.h"

static short want_events_for_kind(enum VK_IO_OP_KIND k)
{
    switch (k) {
        case VK_IO_READ:
        case VK_IO_ACCEPT:
            return POLLIN;
        case VK_IO_WRITE:
            return POLLOUT;
        default:
            return 0;
    }
}

size_t vk_io_batch_sync_run(const struct vk_io_fd_stream* streams,
                            size_t nstreams,
                            struct vk_io_op** completed,
                            size_t* completed_n,
                            struct pollfd* needs_poll,
                            size_t* needs_poll_n)
{
    size_t i;
    size_t ncomp = 0;
    size_t npoll = 0;

    for (i = 0; i < nstreams; ++i) {
        const struct vk_io_fd_stream* s = &streams[i];
        if (!s->q) continue;
        struct vk_io_op* op = vk_io_queue_pop_phys(s->q);
        if (!op) continue;

        /* execute */
        (void)vk_io_exec_rw(op);
        completed[ncomp++] = op;

        if (op->state == VK_IO_OP_NEEDS_POLL) {
            struct pollfd ev;
            ev.fd = op->fd;
            ev.events = want_events_for_kind(op->kind);
            ev.revents = 0;
            needs_poll[npoll++] = ev;
        }
    }

    if (completed_n) *completed_n = ncomp;
    if (needs_poll_n) *needs_poll_n = npoll;
    return ncomp;
}
