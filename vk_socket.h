#ifndef VK_SOCKET_H
#define VK_SOCKET_H

#include <unistd.h>

struct vk_vectoring;
struct vk_pipe;
struct vk_fd;
struct vk_thread; /* forward declare coroutine */
struct socket;	  /* forward declare socket */
struct vk_pipe;	  /* forward declare pipe */
#include "vk_pipe.h"
#include "vk_vectoring.h"

struct vk_buffering;

enum vk_op_type {
	VK_OP_NONE,
	VK_OP_READ,
	VK_OP_WRITE,
	VK_OP_FORWARD,
	VK_OP_FLUSH,
	VK_OP_HUP,
	VK_OP_READHUP,
	VK_OP_TX_CLOSE,
	VK_OP_RX_CLOSE,
	VK_OP_RX_SHUTDOWN,
	VK_OP_TX_SHUTDOWN,
	VK_OP_READABLE,
	VK_OP_WRITABLE,
};
struct vk_block;
void vk_block_init(struct vk_block* block, char* buf, size_t len, int op);
int vk_block_get_op(struct vk_block* block_ptr);
const char* vk_block_get_op_str(struct vk_block* block_ptr);
void vk_block_set_op(struct vk_block* block_ptr, int op);

ssize_t vk_block_commit(struct vk_block* block, ssize_t rc);
size_t vk_block_get_committed(struct vk_block* block);
size_t vk_block_get_uncommitted(struct vk_block* block);
void vk_block_set_uncommitted(struct vk_block* block_ptr, size_t len);
char* vk_block_get_buf(struct vk_block* block_ptr);
void vk_block_set_buf(struct vk_block* block_ptr, char* buf);
size_t vk_block_get_len(struct vk_block* block_ptr);
void vk_block_set_len(struct vk_block* block_ptr, size_t len);

struct vk_io_future* vk_block_get_ioft_rx_pre(struct vk_block* block_ptr);
struct vk_io_future* vk_block_get_ioft_tx_pre(struct vk_block* block_ptr);
struct vk_io_future* vk_block_get_ioft_rx_ret(struct vk_block* block_ptr);
struct vk_io_future* vk_block_get_ioft_tx_ret(struct vk_block* block_ptr);

struct vk_thread* vk_block_get_vk(struct vk_block* block_ptr);
void vk_block_set_vk(struct vk_block* block_ptr, struct vk_thread* blocked_vk);

struct vk_socket;

void vk_socket_init(struct vk_socket* socket_ptr, struct vk_thread* that, struct vk_pipe* rx_ptr,
		    struct vk_pipe* tx_ptr);
size_t vk_socket_size(struct vk_socket* socket_ptr);

/* socket queue poll methods */

struct vk_pipe* vk_socket_get_rx_fd(struct vk_socket* socket_ptr);
struct vk_pipe* vk_socket_get_tx_fd(struct vk_socket* socket_ptr);
struct vk_vectoring* vk_socket_get_rx_vectoring(struct vk_socket* socket_ptr);
struct vk_vectoring* vk_socket_get_tx_vectoring(struct vk_socket* socket_ptr);

/* check EOF flag on socket -- more bytes may still be available to receive from socket */
int vk_socket_eof(struct vk_socket* socket_ptr);
/* no more bytes are available to receive from socket (but EOF may not be set) */
int vk_socket_empty(struct vk_socket* socket_ptr);
/* check EOF flag on socket, and that no more bytes are available to receive from socket */
int vk_socket_nodata(struct vk_socket* socket_ptr);
/* clear EOF flag on socket */
void vk_socket_clear(struct vk_socket* socket_ptr);
/* check EOF flag on write-side */
int vk_socket_eof_tx(struct vk_socket* socket_ptr);
/* no more bytes are available to send from socket (but EOF may not be set) */
int vk_socket_empty_tx(struct vk_socket* socket_ptr);
/* check EOF flag on write side, and that no more bytes are to send */
int vk_socket_nodata_tx(struct vk_socket* socket_ptr);
/* clear EOF flag on write side */
void vk_socket_clear_tx(struct vk_socket* socket_ptr);
/* whether the read pipe has nodata state */
int vk_socket_get_reader_nodata(struct vk_socket* socket_ptr);
/* may perform a readhup op */
int vk_socket_pollhup(struct vk_socket* socket_ptr);

struct vk_block* vk_socket_get_block(struct vk_socket* socket_ptr);
int vk_socket_get_error(struct vk_socket* socket_ptr);
void vk_socket_enqueue_blocked(struct vk_socket* socket_ptr);
int vk_socket_get_enqueued_blocked(struct vk_socket* socket_ptr);
void vk_socket_set_enqueued_blocked(struct vk_socket* socket_ptr, int blocked_enq);
struct vk_socket* vk_socket_next_blocked_socket(struct vk_socket* socket_ptr);

int vk_socket_handle_tx_close(struct vk_socket* socket);
int vk_socket_handle_rx_close(struct vk_socket* socket);

/* handle socket block */
ssize_t vk_socket_handler(struct vk_socket* socket);

/* non-blocking handling made directly available to the vk_thread_io.h vk_readhup() op */
ssize_t vk_socket_handle_readhup(struct vk_socket* socket_ptr);

#define PRblock "<block op=\"%8s\" len=\"%5zu\" committed=\"%5zu\">"

#define ARGblock(block_ptr)                                                                                            \
	vk_block_get_op_str(block_ptr), vk_block_get_len(block_ptr), vk_block_get_committed(block_ptr)

#define vk_block_logf(fmt, ...) ERR("     block: " PRloc " " PRblock ": " fmt, ARGloc, ARGblock(block_ptr), __VA_ARGS__)
#define vk_block_dbgf(fmt, ...) DBG("     block: " PRloc " " PRblock ": " fmt, ARGloc, ARGblock(block_ptr), __VA_ARGS__)
#define vk_block_log(note)                                                                                             \
	ERR("     block: " PRloc " " PRblock ": "                                                                      \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGblock(block_ptr), note)
#define vk_block_dbg(note)                                                                                             \
	DBG("     block: " PRloc " " PRblock ": "                                                                      \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGblock(block_ptr), note)

#define PRsocket                                                                                                       \
	"<socket rx_fd=\"%4i\" tx_fd=\"%4i\" error=\"%4i\" blocked_enq=\"%c\" blocked_op=\"%s\">"                      \
	"\n      \\`rx: " PRvectoring PRvectoring_rx "\n       `tx: " PRvectoring PRvectoring_tx

#define ARGsocket(socket_ptr)                                                                                          \
	vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), vk_pipe_get_fd(vk_socket_get_tx_fd(socket_ptr)),              \
	    vk_socket_get_error(socket_ptr), vk_socket_get_enqueued_blocked(socket_ptr) ? 't' : 'f',                   \
	    ((vk_block_get_op(vk_socket_get_block(socket_ptr)) != 0)                                                   \
		 ? vk_block_get_op_str(vk_socket_get_block(socket_ptr))                                                \
		 : ""),                                                                                                \
	    ARGvectoring(vk_socket_get_rx_vectoring(socket_ptr)),                                                      \
	    ARGvectoring_rx(vk_socket_get_rx_vectoring(socket_ptr)),                                                   \
	    ARGvectoring(vk_socket_get_tx_vectoring(socket_ptr)),                                                      \
	    ARGvectoring_tx(vk_socket_get_tx_vectoring(socket_ptr))

#define vk_socket_logf(fmt, ...)                                                                                       \
	ERR("    socket: " PRloc " " PRsocket ": " fmt, ARGloc, ARGsocket(socket_ptr), __VA_ARGS__)
#define vk_socket_dbgf(fmt, ...)                                                                                       \
	DBG("    socket: " PRloc " " PRsocket ": " fmt, ARGloc, ARGsocket(socket_ptr), __VA_ARGS__)
#define vk_socket_log(note)                                                                                            \
	ERR("    socket: " PRloc " " PRsocket ": "                                                                     \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGsocket(socket_ptr), note)
#define vk_socket_dbg(note)                                                                                            \
	DBG("    socket: " PRloc " " PRsocket ": "                                                                     \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGsocket(socket_ptr), note)
#define vk_socket_perror(string) vk_socket_logf("%s: %s\n", string, strerror(vk_socket_get_error(socket_ptr)))

#endif
