#ifndef VK_VECTORING_H
#define VK_VECTORING_H

#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

struct vk_vectoring {
	char  *buf_start;
	size_t buf_len;
	size_t tx_cursor;
	size_t tx_len;
	struct iovec vector_tx[2];
	struct iovec vector_rx[2];
	int    error; /* errno */
	int    eof;   /* end of file */
	int    rx_poll; /* POLLIN     (last physical read  was not fully satisfied) */
	int    tx_poll; /* POLLOUT    (last physical write was not fully satisfied) */
	int    rx_ready; /* IN  ready (last physical read  was     fully satisfied) */
	int    tx_ready; /* OUT ready (last physical write was     fully satisfied) */
};

/* validate the coherence of the internal buffer address ranges */
void vk_vectoring_validate(struct vk_vectoring *ring);

/* to initialize ring buffer around a buffer */
void vk_vectoring_init(struct vk_vectoring *ring, char *start, size_t len);
#define VK_VECTORING_INIT(ring, buf) vk_vectoring_init(ring, (buf), sizeof (buf))

size_t vk_vectoring_tx_len(const struct vk_vectoring *ring);
size_t vk_vectoring_rx_len(const struct vk_vectoring *ring);

int vk_vectoring_printf(const struct vk_vectoring *ring, const char *label);

char vk_vectoring_rx_pos(const struct vk_vectoring *ring, size_t pos);
char vk_vectoring_tx_pos(const struct vk_vectoring *ring, size_t pos);
int vk_vectoring_tx_line_len(const struct vk_vectoring *ring, size_t *pos_ptr);
size_t vk_vectoring_tx_line_request(const struct vk_vectoring *ring, size_t len);


/* read from file-descriptor to vector-ring */
ssize_t vk_vectoring_read(struct vk_vectoring *ring, int d);
/* write to file-descriptor from vector-ring */
ssize_t vk_vectoring_write(struct vk_vectoring *ring, int d);

/* has error */
int vk_vectoring_has_error(struct vk_vectoring *ring);
/* has EOF */
int vk_vectoring_has_eof(struct vk_vectoring *ring);
/* has EOF and has no data */
int vk_vectoring_has_nodata(struct vk_vectoring *ring);
/* clear error */
void vk_vectoring_clear_error(struct vk_vectoring *ring);
/* clear EOF */
void vk_vectoring_clear_eof(struct vk_vectoring *ring);
/* set error to errno value */
void vk_vectoring_set_error(struct vk_vectoring *ring);
/* mark EOF */
void vk_vectoring_mark_eof(struct vk_vectoring *ring);

/* send from vector-ring to receive-buffer */
ssize_t vk_vectoring_recv(struct vk_vectoring *ring, void *buf, size_t len);
/* receive to vector-ring from send-buffer */
ssize_t vk_vectoring_send(struct vk_vectoring *ring, const void *buf, size_t len);

/* splice data from vector-ring to vector-ring */
ssize_t vk_vectoring_recv_splice(struct vk_vectoring *ring_rx, struct vk_vectoring *ring_tx);

/* read into vector-ring from vector-ring */
ssize_t vk_vectoring_splice(struct vk_vectoring *ring_rx, struct vk_vectoring *ring_tx);

#endif
