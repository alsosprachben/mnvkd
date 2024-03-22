#ifndef VK_SOCKET_H
#define VK_SOCKET_H

#include <unistd.h>

struct vk_vectoring;
struct vk_pipe;
struct vk_fd;
struct vk_thread;    /* forward declare coroutine */
struct socket;  /* forward declare socket */
struct vk_pipe; /* forward declare pipe */

struct vk_buffering;

enum vk_op_type {
	VK_OP_NONE,
	VK_OP_READ,
	VK_OP_WRITE,
	VK_OP_FORWARD,
	VK_OP_FLUSH,
	VK_OP_HUP,
	VK_OP_TX_CLOSE,
	VK_OP_RX_CLOSE,
	VK_OP_READABLE,
	VK_OP_WRITABLE,
};
struct vk_block;
void vk_block_init(struct vk_block *block, char *buf, size_t len, int op);
int vk_block_get_op(struct vk_block *block_ptr);
const char *vk_block_get_op_str(struct vk_block *block_ptr);
void vk_block_set_op(struct vk_block *block_ptr, int op);

ssize_t vk_block_commit(struct vk_block *block, ssize_t rc);
size_t vk_block_get_committed(struct vk_block *block);
size_t vk_block_get_uncommitted(struct vk_block *block);
void vk_block_set_uncommitted(struct vk_block *block_ptr, size_t len);
char *vk_block_get_buf(struct vk_block *block_ptr);
void vk_block_set_buf(struct vk_block *block_ptr, char *buf);
size_t vk_block_get_len(struct vk_block *block_ptr);
void vk_block_set_len(struct vk_block *block_ptr, size_t len);

struct vk_io_future *vk_block_get_ioft_rx_pre(struct vk_block *block_ptr);
struct vk_io_future *vk_block_get_ioft_tx_pre(struct vk_block *block_ptr);
struct vk_io_future *vk_block_get_ioft_rx_ret(struct vk_block *block_ptr);
struct vk_io_future *vk_block_get_ioft_tx_ret(struct vk_block *block_ptr);

struct vk_thread *vk_block_get_vk(struct vk_block *block_ptr);
void vk_block_set_vk(struct vk_block *block_ptr, struct vk_thread *blocked_vk);

struct vk_socket;

void vk_socket_init(struct vk_socket *socket_ptr, struct vk_thread *that, struct vk_pipe *rx_ptr, struct vk_pipe *tx_ptr);
size_t vk_socket_size(struct vk_socket *socket_ptr);

/* socket queue poll methods */

struct vk_pipe *vk_socket_get_rx_fd(struct vk_socket *socket_ptr);
struct vk_pipe *vk_socket_get_tx_fd(struct vk_socket *socket_ptr);
struct vk_vectoring *vk_socket_get_rx_vectoring(struct vk_socket *socket_ptr);
struct vk_vectoring *vk_socket_get_tx_vectoring(struct vk_socket *socket_ptr);
struct vk_block *vk_socket_get_block(struct vk_socket *socket_ptr);
int vk_socket_get_error(struct vk_socket *socket_ptr);
void vk_socket_enqueue_blocked(struct vk_socket *socket_ptr);
int vk_socket_get_enqueued_blocked(struct vk_socket *socket_ptr);
void vk_socket_set_enqueued_blocked(struct vk_socket *socket_ptr, int blocked_enq);
struct vk_socket *vk_socket_next_blocked_socket(struct vk_socket *socket_ptr);

/* blocking poll methods */
int vk_block_get_blocked_rx_fd(struct vk_block *block_ptr);
int vk_block_get_blocked_tx_fd(struct vk_block *block_ptr);
int vk_block_get_blocked_rx_events(struct vk_block *block_ptr);
int vk_block_get_blocked_tx_events(struct vk_block *block_ptr);

int vk_socket_handle_tx_close(struct vk_socket *socket);
int vk_socket_handle_rx_close(struct vk_socket *socket);

/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket *socket);

#endif
