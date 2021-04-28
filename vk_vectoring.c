#include "vk_vectoring.h"

#include <string.h>

int vk_vectoring_vector_within(const struct vk_vectoring *ring, const struct iovec *vector) {
    return ((char *) vector->iov_base) > ring->buf_start && ((char *) vector->iov_base) + vector->iov_len <= ring->buf_start + ring->buf_len;
}

int vk_vectoring_vectors_within(const struct vk_vectoring *ring, const struct iovec vectors[2]) {
    return vk_vectoring_vector_within(ring, &vectors[0]) && vk_vectoring_vector_within(ring, &vectors[1]);
}

/* validate the coherence of the internal buffer address ranges */
void vk_vectoring_validate(struct vk_vectoring *ring) {
    if (
       !vk_vectoring_vectors_within(ring, ring->vector_rx)
    || !vk_vectoring_vectors_within(ring, ring->vector_tx)
    ) {
        ring->error = EINVAL;
    }
}

size_t vk_vectoring_tx_cursor(const struct vk_vectoring *ring) {
    return ring->tx_cursor;
}
size_t vk_vectoring_tx_len(const struct vk_vectoring *ring) {
    return ring->tx_len;
}
size_t vk_vectoring_rx_cursor(const struct vk_vectoring *ring) {
    return (ring->tx_cursor + ring->tx_len) % ring->buf_len;
}
size_t vk_vectoring_rx_len(const struct vk_vectoring *ring) {
    return ring->buf_len - ring->tx_len;
}

/* return error */
int vk_void_return(struct vk_vectoring *ring) {
    if (ring->error != 0) {
        return -1;
    } else {
        return 0;
    }
}

/* return error and size */
ssize_t vk_size_return(struct vk_vectoring *ring, size_t size) {
    if (ring->error == -1) {
        return -1;
    } else {
        if (size != (ssize_t) size) {
            ring->error = EINVAL;
            return -1;
        } else {
            return (ssize_t) size;
        }
    }
}

void vk_vectoring_iovec(struct iovec vectors[2], char *buf_start, size_t buf_len, size_t cursor, size_t len) {
    vectors[0].iov_base     = buf_start + cursor;
    if (cursor + len > buf_len) {
        /* wrapped */
        vectors[0].iov_len  = buf_len - (cursor + len);

        vectors[1].iov_base = buf_start;
        vectors[1].iov_len  = len - vectors[0].iov_len;
    } else {
        /* not wrapped */
        vectors[0].iov_len  = len;

        vectors[1].iov_base = buf_start;
        vectors[1].iov_len  = 0;
    }
}

void vk_vectoring_sync_tx(struct vk_vectoring *ring) {
    vk_vectoring_iovec(ring->vector_tx, ring->buf_start, ring->buf_len, vk_vectoring_tx_cursor(ring), vk_vectoring_tx_len(ring));
}
void vk_vectoring_sync_rx(struct vk_vectoring *ring) {
    vk_vectoring_iovec(ring->vector_rx, ring->buf_start, ring->buf_len, vk_vectoring_rx_cursor(ring), vk_vectoring_rx_len(ring));
}

void vk_vectoring_sync(struct vk_vectoring *ring) {
    vk_vectoring_sync_rx(ring);
    vk_vectoring_sync_tx(ring);
}

/* to initialize ring buffer around a buffer */
void vk_vectoring_init(struct vk_vectoring *ring, char *start, size_t len) {
    ring->buf_start = start;
    ring->buf_len  = len;
    ring->tx_cursor = 0;
    ring->tx_len   = 0;
    vk_vectoring_sync(ring);
    ring->error = 0;
}

/* the number of bytes between start and stop cursors within the ring */
size_t vk_vectoring_len(const struct vk_vectoring *ring, const struct iovec vectors[0]) {
    return vectors[0].iov_len + vectors[1].iov_len;
}

/* the number of filled bytes available to transmit */
size_t vk_vectoring_vector_tx_len(const struct vk_vectoring *ring) {
    return vk_vectoring_len(ring, ring->vector_tx);
}

/* the number of empty bytes available to receive */
size_t vk_vectoring_vector_rx_len(const struct vk_vectoring *ring) {
    return vk_vectoring_len(ring, ring->vector_rx);
}

/* mark unsigned received length, and mark any overflow error */
void vk_vectoring_mark_received(struct vk_vectoring *ring, size_t received) {
        if (received <= vk_vectoring_rx_len(ring)) {
            ring->tx_len = (ring->tx_len + received) % ring->buf_len;
            vk_vectoring_sync(ring);
        } else {
            ring->error = ENOBUFS;
        }
}

/* mark bytes received from vector-ring from return value */
ssize_t vk_vectoring_signed_received(struct vk_vectoring *ring, ssize_t received) {
    if (received < 0) {
        ring->error = errno;
    } else {
        vk_vectoring_mark_received(ring, (size_t) received);
    }
    return received;
}

/* mark unsigned sent length, and mark any overlfow error */
void vk_vectoring_mark_sent(struct vk_vectoring *ring, size_t sent) {
    if (sent <= vk_vectoring_tx_len(ring)) {
        ring->tx_cursor = (ring->tx_cursor + sent) % ring->buf_len;
        vk_vectoring_sync(ring);
    } else {
        ring->error = ENOBUFS;
    }
}

/* mark bytes sent to vector-ring from return value */
ssize_t vk_vectoring_signed_sent(struct vk_vectoring *ring, ssize_t sent) {
    if (sent < 0) {
        ring->error = errno;
    } else {
        vk_vectoring_mark_sent(ring, (size_t) sent);
    }
    return sent;
}

/* read from file-descriptor to vector-ring */
ssize_t vk_vectoring_read(struct vk_vectoring *ring, int d) {
    ssize_t received;

    received = readv(d, ring->vector_tx, 2);

    return vk_vectoring_signed_received(ring, received);
}

/* write to file-descriptor from vector-ring */
ssize_t vk_vectoring_write(struct vk_vectoring *ring, int d) {
    ssize_t sent;

    sent = writev(d, ring->vector_rx, 2);

    return vk_vectoring_signed_sent(ring, sent);
}

/* return the respective lengths of a set of vectors for a specified length */
void vk_vectoring_request(struct iovec vectors[2], size_t lengths[2], size_t len) {
   if (len <= vectors[0].iov_len) {
        /* request satisfied by first vector */
        lengths[0] = len;
        lengths[1] = 0;
    } else if (len <= vectors[0].iov_len + vectors[1].iov_len) {
        /* request satisfied by first and second vectors */
        lengths[0] =       vectors[0].iov_len;
        lengths[1] = len - vectors[0].iov_len;
    } else {
        /* request partially satisfied by both vectors */
        lengths[0] = vectors[0].iov_len;
        lengths[1] = vectors[1].iov_len;
    }
}

/* send from vector-ring to receive-buffer */
ssize_t vk_vectoring_recv(struct vk_vectoring *ring, void *buf, size_t len) {
    ssize_t sent;
    size_t lengths[2];

    vk_vectoring_request(ring->vector_tx, lengths, len);

    if (lengths[0] > 0) {
        memcpy(          buf,               ring->vector_tx[0].iov_base, lengths[0]);
    }
    if (lengths[1] > 0) {
        memcpy(((char *) buf) + lengths[0], ring->vector_tx[1].iov_base, lengths[1]);
    }

    sent = lengths[0] + lengths[1];

    vk_vectoring_mark_sent(ring, sent);

    return vk_size_return(ring, sent);
}

/* receive to vector-ring from send-buffer */
ssize_t vk_vectoring_send(struct vk_vectoring *ring, const void *buf, size_t len) {
    ssize_t received;
    size_t lengths[2];

    vk_vectoring_request(ring->vector_rx, lengths, len);

    if (lengths[0] > 0) {
        memcpy(ring->vector_rx[0].iov_base,           buf,               lengths[0]);
    }
    if (lengths[1] > 0) {
        memcpy(ring->vector_rx[1].iov_base, ((char *) buf) + lengths[0], lengths[1]);
    }

    received = lengths[0] + lengths[1];

    vk_vectoring_mark_received(ring, received);

    return vk_size_return(ring, received);
}

