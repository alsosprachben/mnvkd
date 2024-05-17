#ifndef VK_VECTORING_H
#define VK_VECTORING_H

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

struct vk_vectoring;

/* validate the coherence of the internal buffer address ranges */
void vk_vectoring_validate(struct vk_vectoring* ring);

size_t vk_vectoring_tx_cursor(const struct vk_vectoring* ring);
size_t vk_vectoring_tx_len(const struct vk_vectoring* ring);
size_t vk_vectoring_rx_cursor(const struct vk_vectoring* ring);
size_t vk_vectoring_rx_len(const struct vk_vectoring* ring);

/* to initialize ring buffer around a buffer */
void vk_vectoring_init(struct vk_vectoring* ring, char* start, size_t len);
#define VK_VECTORING_INIT(ring, buf) vk_vectoring_init(ring, (buf), sizeof(buf))

/* the size of the ring buffer itself */
size_t vk_vectoring_buf_len(const struct vk_vectoring* ring);

/* debug access */
int vk_vectoring_rx_buf1_len(const struct vk_vectoring* ring);
int vk_vectoring_rx_buf2_len(const struct vk_vectoring* ring);
int vk_vectoring_tx_buf1_len(const struct vk_vectoring* ring);
int vk_vectoring_tx_buf2_len(const struct vk_vectoring* ring);
char* vk_vectoring_rx_buf1(const struct vk_vectoring* ring);
char* vk_vectoring_rx_buf2(const struct vk_vectoring* ring);
char* vk_vectoring_tx_buf1(const struct vk_vectoring* ring);
char* vk_vectoring_tx_buf2(const struct vk_vectoring* ring);

int vk_vectoring_printf(const struct vk_vectoring* ring, const char* label);

char vk_vectoring_rx_pos(const struct vk_vectoring* ring, size_t pos);
char vk_vectoring_tx_pos(const struct vk_vectoring* ring, size_t pos);
int vk_vectoring_tx_line_len(const struct vk_vectoring* ring, size_t* pos_ptr);
size_t vk_vectoring_tx_line_request(const struct vk_vectoring* ring, size_t len);

/* accept from listen file-descriptor to vector-ring */
ssize_t vk_vectoring_accept(struct vk_vectoring* ring, int d);
/* read from file-descriptor to vector-ring */
ssize_t vk_vectoring_read(struct vk_vectoring* ring, int d);
/* write to file-descriptor from vector-ring */
ssize_t vk_vectoring_write(struct vk_vectoring* ring, int d);
/* close file descriptor */
ssize_t vk_vectoring_close(struct vk_vectoring* ring, int d);
/* shutdown read-side of socket file descriptor */
ssize_t vk_vectoring_rx_shutdown(struct vk_vectoring* ring, int d);
/* shutdown write-side of socket file descriptor */
ssize_t vk_vectoring_tx_shutdown(struct vk_vectoring* ring, int d);
/* not ready to receive */
int vk_vectoring_rx_is_blocked(const struct vk_vectoring* ring);
void vk_vectoring_set_rx_blocked(struct vk_vectoring* ring, int rx_blocked);

/* not ready to send */
int vk_vectoring_tx_is_blocked(const struct vk_vectoring* ring);
void vk_vectoring_set_tx_blocked(struct vk_vectoring* ring, int tx_blocked);

/* the number of filled bytes available to transmit */
size_t vk_vectoring_vector_tx_len(const struct vk_vectoring* ring);

/* the number of empty bytes available to receive */
size_t vk_vectoring_vector_rx_len(const struct vk_vectoring* ring);

/* has been marked closed */
int vk_vectoring_is_closed(const struct vk_vectoring* ring);

/* has error */
int vk_vectoring_has_error(struct vk_vectoring* ring);
/* has EOF */
int vk_vectoring_has_eof(struct vk_vectoring* ring);
/* no bytes in transmit buffer */
int vk_vectoring_is_empty(struct vk_vectoring* ring);
/* has EOF and has no data */
int vk_vectoring_has_nodata(struct vk_vectoring* ring);
/* coroutines should be affected */
int vk_vectoring_has_effect(struct vk_vectoring* ring);
/* trigger effect in coroutines */
void vk_vectoring_trigger_effect(struct vk_vectoring* ring);
/* clear effect */
void vk_vectoring_clear_effect(struct vk_vectoring* ring);
/* return effect and clear it */
int vk_vectoring_handle_effect(struct vk_vectoring* ring);
/* clear error */
void vk_vectoring_clear_error(struct vk_vectoring* ring);
/* clear EOF */
void vk_vectoring_clear_eof(struct vk_vectoring* ring);
/* set error to errno value */
void vk_vectoring_set_error(struct vk_vectoring* ring);
/* mark EOF */
int vk_vectoring_mark_eof(struct vk_vectoring* ring);
/* mark closed */
int vk_vectoring_mark_closed(struct vk_vectoring* ring);

/* send from vector-ring to receive-buffer */
ssize_t vk_vectoring_recv(struct vk_vectoring* ring, void* buf, size_t len);
/* receive to vector-ring from send-buffer */
ssize_t vk_vectoring_send(struct vk_vectoring* ring, const void* buf, size_t len);

/* splice data from vector-ring to vector-ring */
ssize_t vk_vectoring_recv_splice(struct vk_vectoring* ring_rx, struct vk_vectoring* ring_tx, ssize_t len);

/* read into vector-ring from vector-ring */
ssize_t vk_vectoring_splice(struct vk_vectoring* ring_rx, struct vk_vectoring* ring_tx, ssize_t len);

/* produce EOF or error */
ssize_t vk_vectoring_hup(struct vk_vectoring* ring);

/* consume EOF or error */
ssize_t vk_vectoring_readhup(struct vk_vectoring* ring);

#define PRvectoring                                                                                                    \
	"<vectoring"                                                                                                   \
	" rx=\"%zu+%zu/%zu\""                                                                                          \
	" tx=\"%zu+%zu/%zu\""                                                                                          \
	" err=\"%s\""                                                                                                  \
	" block=\"%c%c\""                                                                                              \
	" eof=\"%c\""                                                                                                  \
	" nodata=\"%c\""                                                                                               \
	" closed=\"%c\""                                                                                               \
	" effect=\"%c\""                                                                                               \
	">"
#define ARGvectoring(ring)                                                                                             \
	vk_vectoring_rx_cursor(ring), vk_vectoring_rx_len(ring), vk_vectoring_buf_len(ring),                           \
	    vk_vectoring_tx_cursor(ring), vk_vectoring_tx_len(ring), vk_vectoring_buf_len(ring),                       \
	    strerror(vk_vectoring_has_error(ring)), vk_vectoring_rx_is_blocked(ring) ? 'r' : ' ',                      \
	    vk_vectoring_tx_is_blocked(ring) ? 'w' : ' ', vk_vectoring_has_eof(ring) ? 't' : 'f',                      \
	    vk_vectoring_has_nodata(ring) ? 't' : 'f', vk_vectoring_is_closed(ring) ? 't' : 'f',                       \
	    vk_vectoring_has_effect(ring) ? 't' : 'f'

#define PRvectoring_tx "<vectoring_tx len=\"%5zu\">"
#define ARGvectoring_tx(ring) vk_vectoring_tx_len(ring)

#define PRvectoring_rx "<vectoring_rx len=\"%5zu\">"
#define ARGvectoring_rx(ring) vk_vectoring_rx_len(ring)

#define vk_vectoring_logf(fmt, ...)                                                                                    \
	ERR(" vectoring: " PRloc " " PRvectoring ": " fmt, ARGloc, ARGvectoring(ring), __VA_ARGS__)
#define vk_vectoring_dbgf(fmt, ...)                                                                                    \
	DBG(" vectoring: " PRloc " " PRvectoring ": " fmt, ARGloc, ARGvectoring(ring), __VA_ARGS__)
#define vk_vectoring_log(note)                                                                                         \
	ERR(" vectoring: " PRloc " " PRvectoring ": "                                                                  \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGvectoring(ring), note)
#define vk_vectoring_dbg(note)                                                                                         \
	DBG(" vectoring: " PRloc " " PRvectoring ": "                                                                  \
	    "%s\n",                                                                                                    \
	    ARGloc, ARGvectoring(ring), note)
#define vk_vectoring_perror(string) vk_vectoring_logf("%s: %s\n", string, strerror(errno))
#define vk_vectoring_logf_tx(fmt, ...)                                                                                 \
	ERR(" vectoring: " PRloc " " PRvectoring " " PRvectoring_tx ": " fmt, ARGloc, ARGvectoring(ring),              \
	    ARGvectoring_tx(ring), __VA_ARGS__)
#define vk_vectoring_dbgf_tx(fmt, ...)                                                                                 \
	DBG(" vectoring: " PRloc " " PRvectoring " " PRvectoring_tx ": " fmt, ARGloc, ARGvectoring(ring),              \
	    ARGvectoring_tx(ring), __VA_ARGS__)
#define vk_vectoring_logf_rx(fmt, ...)                                                                                 \
	ERR(" vectoring: " PRloc " " PRvectoring " " PRvectoring_rx ": " fmt, ARGloc, ARGvectoring(ring),              \
	    ARGvectoring_rx(ring), __VA_ARGS__)
#define vk_vectoring_dbgf_rx(fmt, ...)                                                                                 \
	DBG(" vectoring: " PRloc " " PRvectoring " " PRvectoring_rx ": " fmt, ARGloc, ARGvectoring(ring),              \
	    ARGvectoring_rx(ring), __VA_ARGS__)

#endif
