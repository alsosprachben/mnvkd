#include "vk_thread_io.h"

#include "vk_fd.h"
#include "vk_proc_local.h"
#include "vk_socket.h"
#include "vk_socket_s.h"
#include "vk_thread.h"
#include "vk_vectoring.h"

struct vk_io_op* vk_thread_queue_io_op(struct vk_thread* that,
                                       struct vk_socket* socket_ptr,
                                       struct vk_io_queue* queue_ptr)
{
    struct vk_io_op* op_ptr;

    if (vk_socket_prepare_block_op(socket_ptr, &op_ptr) == -1) {
        return NULL;
    }

    if (vk_io_op_get_fd(op_ptr) < 0) {
        /* Virtual peers run immediately via vk_wait/vk_unblock; no queuing required. */
        vk_socket_handle_block(socket_ptr);
        return op_ptr;
    }

    if (!queue_ptr) {
        queue_ptr = &socket_ptr->io_queue_fallback;
        if (!socket_ptr->io_queue_fallback_init) {
            vk_io_queue_init(queue_ptr);
            socket_ptr->io_queue_fallback_init = 1;
        }
        vk_socket_set_io_queue(socket_ptr, queue_ptr);
    }

    vk_io_queue_push(queue_ptr, op_ptr);
    op_ptr->tag1 = queue_ptr;

    int blocked_op = vk_block_get_op(vk_socket_get_block(socket_ptr));
    switch (blocked_op) {
        case VK_OP_READ:
        case VK_OP_READABLE:
            vk_vectoring_set_rx_blocked(vk_socket_get_rx_vectoring(socket_ptr), 1);
            break;
        case VK_OP_WRITE:
        case VK_OP_FLUSH:
        case VK_OP_HUP:
        case VK_OP_TX_CLOSE:
        case VK_OP_TX_SHUTDOWN:
        case VK_OP_FORWARD:
            vk_vectoring_set_tx_blocked(vk_socket_get_tx_vectoring(socket_ptr), 1);
            break;
        default:
            break;
    }

    DBG("queue_io_op: op=%d fd=%d queue=%p len=%zu\n",
        blocked_op, vk_io_op_get_fd(op_ptr), (void*)queue_ptr, vk_io_op_get_len(op_ptr));
    vk_socket_handle_block(socket_ptr);

    return op_ptr;
}

void vk_thread_io_complete_op(struct vk_socket* socket_ptr,
                              struct vk_io_queue* queue_ptr,
                              struct vk_io_op* op_ptr)
{
    if (!op_ptr) return;

    if (queue_ptr && op_ptr->tag1 == queue_ptr && op_ptr->q_elem.tqe_prev != NULL) {
        vk_io_queue_remove(queue_ptr, op_ptr);
        op_ptr->tag1 = NULL;
    }

    DBG("io_complete: fd=%d kind=%d state=%d res=%zd err=%d\n",
        vk_io_op_get_fd(op_ptr),
        vk_io_op_get_kind(op_ptr),
        vk_io_op_get_state(op_ptr),
        op_ptr->res,
        op_ptr->err);

    vk_socket_apply_block_op(socket_ptr, op_ptr);

    if (op_ptr->state != VK_IO_OP_NEEDS_POLL && op_ptr->state != VK_IO_OP_EAGAIN) {
        struct vk_thread* owner_thread = vk_io_op_get_thread(op_ptr);
        if (owner_thread) {
            struct vk_proc_local* proc_local_ptr = vk_get_proc_local(owner_thread);
            if (proc_local_ptr) {
                vk_proc_local_drop_deferred(proc_local_ptr, owner_thread);
            }
        }
    }
}
