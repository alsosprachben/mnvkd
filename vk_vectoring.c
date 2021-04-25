#include "vk_vectoring.h"

#include <string.h>

/* validate the coherence of the internal buffer address ranges */
void vk_vectoring_validate(struct vk_vectoring *ring) {
    if (ring->buf_stop < ring->buf_start) {
        ring->error = EINVAL;
    }
    if (ring->buf_start > ring->filled_start || ring->filled_start > ring->buf_stop) {
        ring->error = EINVAL;
    }
    if (ring->buf_start > ring->filled_stop  || ring->filled_stop  > ring->buf_stop) {
        ring->error = EINVAL;
    }
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
    if (ring->error != -1) {
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

/* to initialize ring buffer around a buffer */
void vk_vectoring_init(struct vk_vectoring *ring, char *start, char *stop) {
    ring->buf_start = start;
    ring->buf_stop  = stop;
    ring->filled_start = start;
    ring->filled_stop  = start;
}

/* the number of total bytes in the buffer to transmit or receive */
size_t vk_vectoring_buf_length(struct vk_vectoring *ring) {
    if (ring->buf_start < ring->buf_stop) {
        return ring->buf_stop - ring->buf_start;
    } else {
        ring->error = EINVAL;
        return 0;
    }
}

/* the number of bytes between start and stop cursors within the ring */
size_t vk_vectoring_length(struct vk_vectoring *ring, const char *start, const char *stop) {
    if (start < stop) {
        /* does not wrap */
        return stop - start;
    } else if (start + vk_vectoring_buf_length(ring) < stop) {
        /* does wrap one cycle */
        return stop - start + vk_vectoring_buf_length(ring);
    } else {
        ring->error = EINVAL;
        return 0;
    }
}

/* the number of filled bytes available to transmit */
size_t vk_vectoring_tx_length(struct vk_vectoring *ring) {
    return vk_vectoring_length(ring, ring->filled_start, ring->filled_stop);
}

/* the number of empty bytes available to receive */
size_t vk_vectoring_rx_length(struct vk_vectoring *ring) {
    return vk_vectoring_length(ring, ring->filled_stop, ring->filled_start);
}

/* `struct iovec` view of vector-ring start and stop cursors */
void vk_vectoring_iovec(const struct vk_vectoring *ring, struct iovec vectors[2], char *start, char *stop) {
    vectors[0].iov_base = ring->filled_stop;
    if (start > stop) {
        /* does not wrap */
        vectors[0].iov_len  = start - stop;

        vectors[1].iov_base = start;
        vectors[1].iov_len  = 0;
    } else {
        /* does wrap */
        vectors[0].iov_len = ring->buf_stop - stop;

        vectors[1].iov_base =         ring->buf_start;
        vectors[1].iov_len  = start - ring->buf_start;
    }
}

/* `struct iovec` view of vector-ring empty buffer to receive */
void vk_vectoring_rx_iovec(struct vk_vectoring *ring, struct iovec vectors[2]) {
    vk_vectoring_iovec(ring, vectors, ring->filled_start, ring->filled_stop);
}

/* `struct iovec` view of vector-ring filled buffer to transmit */
void vk_vectoring_tx_iovec(struct vk_vectoring *ring, struct iovec vectors[2]) {
    vk_vectoring_iovec(ring, vectors, ring->filled_stop, ring->filled_start);
}

/* mark bytes received from vector-ring from return value */
ssize_t vk_vectoring_received(struct vk_vectoring *ring, ssize_t received) {
    if (received < 0) {
        ring->error = errno;
    } else {
        ring->filled_stop += received;
        if (ring->filled_stop > ring->buf_stop) {
            /* wrapped */
            ring->filled_stop -= vk_vectoring_buf_length(ring);
        }
    }
    return received;
}

/* mark bytes sent to vector-ring from return value */
ssize_t vk_vectoring_sent(struct vk_vectoring *ring, ssize_t sent) {
    if (sent < 0) {
        ring->error = errno;
    } else {
        ring->filled_start += sent;
        if (ring->filled_stop > ring->buf_stop) {
            /* wrapped */
            ring->filled_start -= vk_vectoring_buf_length(ring);
        }
    }
    return sent;
}

/* read from file-descriptor to vector-ring */
ssize_t vk_vectoring_read(struct vk_vectoring *ring, int d) {
    ssize_t received;
    struct iovec vectors[2];

    vk_vectoring_rx_iovec(ring, vectors);

    received = readv(d, vectors, 2);

    return vk_vectoring_received(ring, received);
}

/* write to file-descriptor from vector-ring */
ssize_t vk_vectoring_write(struct vk_vectoring *ring, int d) {
    ssize_t sent;
    struct iovec vectors[2];

    vk_vectoring_tx_iovec(ring, vectors);

    sent = writev(d, vectors, 2);

    return vk_vectoring_sent(ring, sent);
}

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
    struct iovec vectors[2];
    size_t lengths[2];

    vk_vectoring_tx_iovec(ring, vectors);
    vk_vectoring_request(vectors, lengths, len);

    if (lengths[0] > 0) {
        memcpy(          buf,               vectors[0].iov_base, lengths[0]);
    }
    if (lengths[1] > 0) {
        memcpy(((char *) buf) + lengths[0], vectors[1].iov_base, lengths[1]);
    }

    sent = lengths[0] + lengths[1];

    return vk_vectoring_sent(ring, sent);
}

/* receive to vector-ring from send-buffer */
ssize_t vk_vectoring_send(struct vk_vectoring *ring, const void *buf, size_t len) {
    ssize_t received;
    struct iovec vectors[2];
    size_t lengths[2];

    vk_vectoring_rx_iovec(ring, vectors);
    vk_vectoring_request(vectors, lengths, len);

    if (lengths[0] > 0) {
        memcpy(vectors[0].iov_base,           buf,               lengths[0]);
    }
    if (lengths[1] > 0) {
        memcpy(vectors[1].iov_base, ((char *) buf) + lengths[0], lengths[1]);
    }

    received = lengths[0] + lengths[1];

    return vk_vectoring_received(ring, received);
}

