#ifndef VK_VECTORING_H
#define VK_VECTORING_H

#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

struct vk_vectoring {
	char *buf_start;
	char *buf_stop;
	char *filled_start;
	char *filled_stop;
    int error; /* errno */
};

/* validate the coherence of the internal buffer address ranges */
void vk_vectoring_validate(struct vk_vectoring *ring);

/* the number of total bytes in the buffer to transmit or receive */
size_t vk_vectoring_buf_length(struct vk_vectoring *ring);
/* the number of filled bytes available to transmit */
size_t vk_vectoring_tx_length(struct vk_vectoring *ring);
/* the number of empty bytes available to receive */
size_t vk_vectoring_rx_length(struct vk_vectoring *ring);

/* `struct iovec` view of vector-ring empty buffer to receive */
void vk_vectoring_rx(struct vk_vectoring *ring, struct iovec vectors[2]);
/* `struct iovec` view of vector-ring filled buffer to transmit */
void vk_vectoring_tx(struct vk_vectoring *ring, struct iovec vectors[2]);

/* mark bytes received from vector-ring from return value */
ssize_t vk_vectoring_received(struct vk_vectoring *ring, ssize_t received);
/* mark bytes sent to vector-ring from return value */
ssize_t vk_vectoring_sent(struct vk_vectoring *ring, ssize_t sent);

/* read from file-descriptor to vector-ring */
ssize_t vk_vectoring_read(struct vk_vectoring *ring, int d);
/* write to file-descriptor from vector-ring */
ssize_t vk_vectoring_write(struct vk_vectoring *ring, int d);

/* send from vector-ring to receive-buffer */
ssize_t vk_vectoring_recv(struct vk_vectoring *ring, void *buf, size_t len);
/* receive to vector-ring from send-buffer */
ssize_t vk_vectoring_send(struct vk_vectoring *ring, void *buf, size_t len);


struct vk_buffering {
	char buf[4096 * 4];
	struct vk_vectoring ring;
};

struct vk_socket {
	struct vk_buffering rx;
	struct vk_buffering tx;
    int error; /* errno */
};

#endif
