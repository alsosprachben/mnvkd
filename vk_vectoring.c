/* Copyright 2022 BCW. All Rights Reserved. */
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "vk_vectoring.h"
#include "vk_vectoring_s.h"
#include "vk_debug.h"
#include "vk_wrapguard.h"

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

int vk_vectoring_printf(const struct vk_vectoring *ring, const char *label) {
	return dprintf(
		2,
		"%s: rx(%zu+%zu/%zu) tx(%zu+%zu/%zu) rx[0].iov_len = %zu, rx[1].iov_len = %zu, tx[0].iov_len = %zu, tx[1].iov_len = %zu, rx buffer = \"%.*s%.*s\", tx buffer = \"%.*s%.*s\"\n",
		label,
		vk_vectoring_rx_cursor(ring), vk_vectoring_rx_len(ring), vk_vectoring_buf_len(ring),
		vk_vectoring_tx_cursor(ring), vk_vectoring_tx_len(ring), vk_vectoring_buf_len(ring),
		ring->vector_rx[0].iov_len,
		ring->vector_rx[1].iov_len,
		ring->vector_tx[0].iov_len,
		ring->vector_tx[1].iov_len,
        (int) ring->vector_rx[0].iov_len, (char *) ring->vector_rx[0].iov_base,
        (int) ring->vector_rx[1].iov_len, (char *) ring->vector_rx[1].iov_base,
        (int) ring->vector_tx[0].iov_len, (char *) ring->vector_tx[0].iov_base,
        (int) ring->vector_tx[1].iov_len, (char *) ring->vector_tx[1].iov_base
	);
}
int vk_vectoring_rx_buf1_len(const struct vk_vectoring *ring) {
    return (int) ring->vector_rx[0].iov_len;
}
int vk_vectoring_rx_buf2_len(const struct vk_vectoring *ring) {
    return (int) ring->vector_rx[1].iov_len;
}
int vk_vectoring_tx_buf1_len(const struct vk_vectoring *ring) {
    return (int) ring->vector_tx[0].iov_len;
}
int vk_vectoring_tx_buf2_len(const struct vk_vectoring *ring) {
    return (int) ring->vector_tx[1].iov_len;
}
char *vk_vectoring_rx_buf1(const struct vk_vectoring *ring) {
    return (char *) ring->vector_rx[0].iov_base;
}
char *vk_vectoring_rx_buf2(const struct vk_vectoring *ring) {
    return (char *) ring->vector_rx[1].iov_base;
}
char *vk_vectoring_tx_buf1(const struct vk_vectoring *ring) {
    return (char *) ring->vector_tx[0].iov_base;
}
char *vk_vectoring_tx_buf2(const struct vk_vectoring *ring) {
    return (char *) ring->vector_tx[1].iov_base;
}

char vk_vectoring_rx_pos(const struct vk_vectoring *ring, size_t pos) {
	size_t buf_pos;

	buf_pos = (vk_vectoring_rx_cursor(ring) + pos) % ring->buf_len;

	return ring->buf_start[buf_pos];
}

char vk_vectoring_tx_pos(const struct vk_vectoring *ring, size_t pos) {
	size_t buf_pos;

	buf_pos = (vk_vectoring_tx_cursor(ring) + pos) % ring->buf_len;

	return ring->buf_start[buf_pos];
}

int vk_vectoring_tx_line_len(const struct vk_vectoring *ring, size_t *pos_ptr) {
	size_t len;
	size_t i;

	len = vk_vectoring_tx_len(ring);

	for (i = 0; i < len; i++) {
		if (vk_vectoring_tx_pos(ring, i) == '\n') {
			*pos_ptr = i + 1;
			return 1;
		}
	}

	return 0;
}

size_t vk_vectoring_tx_line_request(const struct vk_vectoring *ring, size_t len) {
	int rc;
	size_t line_len;

	rc = vk_vectoring_tx_line_len(ring, &line_len);
	if (rc) {
		return line_len;
	} else {
		return len;
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
		vectors[0].iov_len  = buf_len - cursor;

		vectors[1].iov_base = buf_start;
		vectors[1].iov_len  = len - vectors[0].iov_len;
	} else {
		/* not wrapped */
		vectors[0].iov_len  = len;

		/* vectors[1].iov_base = buf_start + len; */
        vectors[1].iov_base = "";
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
	ring->eof = 0;
	ring->rx_blocked = 0;
	ring->tx_blocked = 0;
	ring->effect = 0;
}

/* the size of the ring buffer itself */
size_t vk_vectoring_buf_len(const struct vk_vectoring *ring) {
    return ring->buf_len;
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

/* not ready to receive */
int vk_vectoring_rx_is_blocked(const struct vk_vectoring *ring) {
	return ring->rx_blocked;
}

/* not ready to send */
int vk_vectoring_tx_is_blocked(const struct vk_vectoring *ring) {
	return ring->tx_blocked;
}

/* has been marked closed */
int vk_vectoring_is_closed(const struct vk_vectoring *ring) {
    return ring->closed;
}

/* has error */
int vk_vectoring_has_error(struct vk_vectoring *ring) {
	return ring->error;
}

/* has EOF */
int vk_vectoring_has_eof(struct vk_vectoring *ring) {
	return ring->eof;
}

/* has EOF and has no data */
int vk_vectoring_has_nodata(struct vk_vectoring *ring) {
	return ring->eof && vk_vectoring_vector_tx_len(ring) == 0;
}

/* coroutines should be affected */
int vk_vectoring_has_effect(struct vk_vectoring *ring) {
	return ring->effect;
}

/* trigger effect in coroutines */
void vk_vectoring_trigger_effect(struct vk_vectoring *ring) {
	ring->effect = 1;
	vk_vectoring_dbg("trigger effect");
}

/* clear effect */
void vk_vectoring_clear_effect(struct vk_vectoring *ring) {
	ring->effect = 0;
	vk_vectoring_dbg("clear effect");
}

/* return effect and clear it */
int vk_vectoring_handle_effect(struct vk_vectoring *ring) {
    if (ring->effect) {
        ring->effect = 0;
        return 1;
    } else {
        return 0;
    }
}

/* clear error */
void vk_vectoring_clear_error(struct vk_vectoring *ring) {
	if (ring->error != 0) {
		ring->error = 0;
		ring->effect = 1;
		vk_vectoring_dbg("clear error");
	}
}

/* clear EOF */
void vk_vectoring_clear_eof(struct vk_vectoring *ring) {
	if (ring->eof != 0) {
		ring->eof = 0;
		ring->effect = 1;
		vk_vectoring_dbg("clear eof");
	}
}

/* set error to errno value */
void vk_vectoring_set_error(struct vk_vectoring *ring) {
	if (ring->error != errno) {
		ring->error = errno;
		ring->effect = 1;
		vk_vectoring_dbg("update error");
	}
}

/* mark EOF */
int vk_vectoring_mark_eof(struct vk_vectoring *ring) {
	if (ring->eof == 0) {
		ring->effect = 1;
		ring->eof = 1;
		vk_vectoring_dbg("set eof");
		return 1;
	} else {
		return 0;
	}
}

/* mark closed */
int vk_vectoring_mark_closed(struct vk_vectoring *ring) {
    if (ring->closed == 0) {
        ring->effect = 1;
        ring->closed = 1;
        vk_vectoring_dbg("set closed");
        return 1;
    } else {
        return 0;
    }
}

/* mark unsigned received length, and mark any overflow error */
void vk_vectoring_mark_received(struct vk_vectoring *ring, size_t received) {
	if (received == 0) {
		return;
	}

	if (received < vk_vectoring_rx_len(ring)) {
		ring->tx_len = (ring->tx_len + received) % ring->buf_len;
		vk_vectoring_sync(ring);
	} else if (received == vk_vectoring_rx_len(ring)) {
		ring->tx_len = ring->buf_len;
		vk_vectoring_sync(ring);
	} else {
		ring->error = ENOBUFS;
	}
	ring->effect = 1;
	vk_vectoring_dbg("received");
}

/* mark bytes received from vector-ring from return value */
ssize_t vk_vectoring_signed_received(struct vk_vectoring *ring, ssize_t received) {
	if (received < 0) {
		ring->error = errno;
        errno = 0;
	} else {
		vk_vectoring_mark_received(ring, (size_t) received);
	}
	return received;
}

/* mark unsigned sent length, and mark any overlfow error */
void vk_vectoring_mark_sent(struct vk_vectoring *ring, size_t sent) {
	if (sent == 0) {
		return;
	}

	if (sent <= vk_vectoring_tx_len(ring)) {
		ring->tx_cursor = (ring->tx_cursor + sent) % ring->buf_len;
		ring->tx_len -= sent;
		vk_vectoring_sync(ring);
	} else {
		ring->error = ENOBUFS;
	}
	ring->effect = 1;
}

/* mark bytes sent to vector-ring from return value */
ssize_t vk_vectoring_signed_sent(struct vk_vectoring *ring, ssize_t sent) {
	if (sent < 0) {
		ring->error = errno;
        errno = 0;
	} else {
		vk_vectoring_mark_sent(ring, (size_t) sent);
	}
	return sent;
}

#include "vk_accepted.h"
#include "vk_accepted_s.h"
ssize_t vk_vectoring_accept(struct vk_vectoring *ring, int d) {
    ssize_t received;
    int fd;
    struct vk_accepted accepted;

    if (vk_vectoring_rx_len(ring) < vk_accepted_alloc_size()) {
        vk_vectoring_dbgf_rx("%s\n", "Not enough buffer space to accept a new connection.");
        return 0;
    }

    fd = vk_accepted_accept(&accepted, d);
    if (fd == -1) {
        if (errno == EAGAIN) {
            ring->rx_blocked = 1;
        }
        return -1;
    } else {
        ring->rx_blocked = 0;
        return vk_vectoring_send(ring, &accepted, vk_accepted_alloc_size());
    }
}

/* read from file-descriptor to vector-ring */
ssize_t vk_vectoring_read(struct vk_vectoring *ring, int d) {
	ssize_t received;

	received = readv(d, ring->vector_rx, 2);
	vk_vectoring_dbgf_rx("readv(%i, %p, %i) = %zi\n", d, ring->vector_rx, 2, received);

	if (received == -1) {
		if (errno == EAGAIN) {
			ring->rx_blocked = 1;
		}
	} else if (received < vk_vectoring_vector_rx_len(ring)) {
		/* read request not fully satisfied */
		ring->rx_blocked = 1;
	} else {
		/* read request     fully satisfied */
		ring->rx_blocked = 0;
	}

	if (received == 0) { /* only when EOF */
		ring->eof = 1;
	} else if (received > 0) { /* when EOF may not be set */
		ring->eof = 0;
	} else {
		/* leave errors */
	}

	return vk_vectoring_signed_received(ring, received);
}

/* write to file-descriptor from vector-ring */
ssize_t vk_vectoring_write(struct vk_vectoring *ring, int d) {
	ssize_t sent;

	sent = writev(d, ring->vector_tx, 2);
	vk_vectoring_dbgf_tx("writev(%i, %p, %i) = %zi\n", d, ring->vector_tx, 2, sent);

	if (sent == -1) {
		if (errno == EAGAIN) {
			ring->tx_blocked = 1;
		}
	} else if (sent < vk_vectoring_vector_tx_len(ring)) {
		/* write request not fully satisfied */
		ring->tx_blocked = 1;
	} else {
		/* write request     fully satisifed */
		ring->tx_blocked = 0;
	}

	return vk_vectoring_signed_sent(ring, sent);
}

/* close file descriptor */
ssize_t vk_vectoring_close(struct vk_vectoring *ring, int d) {
	int rc;
	
	rc = close(d);
	vk_vectoring_dbgf("close(%i) = %i\n", d, rc);
	if (rc == EINTR) {
		rc = close(d);
    	vk_vectoring_dbgf("close[2](%i) = %i\n", d, rc);
	}
	if (rc == -1) {
		ring->error = errno;
	}

    ring->effect = 1;
	return vk_vectoring_signed_sent(ring, rc);
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
    int rc;

	vk_vectoring_request(ring->vector_tx, lengths, len);

	if (lengths[0] > 0) {
		memcpy(          buf,               ring->vector_tx[0].iov_base, lengths[0]);
	}
	if (lengths[1] > 0) {
		memcpy(((char *) buf) + lengths[0], ring->vector_tx[1].iov_base, lengths[1]);
	}

    rc = vk_wrapguard_add(lengths[0], lengths[1]);
    if (rc == -1) {
        return -1;
    }
	sent = (ssize_t) lengths[0] + (ssize_t) lengths[1];

	vk_vectoring_mark_sent(ring, sent);

    vk_vectoring_dbgf("recv of %zi/%zu for \"%.*s\"\n", sent, len, (int) sent, (char *) buf);

    return vk_size_return(ring, sent);
}

/* receive to vector-ring from send-buffer */
ssize_t vk_vectoring_send(struct vk_vectoring *ring, const void *buf, size_t len) {
	ssize_t received;
	size_t lengths[2];
    int rc;

	if (len > 0 && ring->eof) {
		/* block writes until EOF is cleared */
		return 0;
	}

	vk_vectoring_request(ring->vector_rx, lengths, len);

	if (lengths[0] > 0) {
		memcpy(ring->vector_rx[0].iov_base,           buf,               lengths[0]);
	}
	if (lengths[1] > 0) {
		memcpy(ring->vector_rx[1].iov_base, ((char *) buf) + lengths[0], lengths[1]);
	}

    rc = vk_wrapguard_add(lengths[0], lengths[1]);
    if (rc == -1) {
        return -1;
    }
	received = (ssize_t) lengths[0] + (ssize_t) lengths[1];

	vk_vectoring_mark_received(ring, received);

    vk_vectoring_dbgf("send of %zi/%zu for \"%.*s\"\n", received, len, (int) received, (char *) buf);

    return vk_size_return(ring, received);
}

/* splice data from vector-ring to vector-ring -- this is the internal read/write op -- if len is < 0, splice all available bytes */
ssize_t vk_vectoring_recv_splice(struct vk_vectoring *ring_rx, struct vk_vectoring *ring_tx, ssize_t len) {
	size_t received;
	size_t lengths[2];
	size_t len_rx, len_tx, len_final;

	len_rx = vk_vectoring_vector_rx_len(ring_rx);
	len_tx = vk_vectoring_vector_tx_len(ring_tx);
	len_final = len_rx < len_tx ? len_rx : len_tx;
	if (len >= 0 && len < len_final) {
        len_final = len;
    }

	vk_vectoring_request(ring_rx->vector_rx, lengths, len_final);

	if (lengths[0] > 0) {
		received = vk_vectoring_recv(ring_tx, ring_rx->vector_rx[0].iov_base, lengths[0]);
		if (received != lengths[0]) {
			errno = EINVAL;
			return -1;
		}
	}
	if (lengths[1] > 0) {
		received = vk_vectoring_recv(ring_tx, ring_rx->vector_rx[1].iov_base, lengths[1]);
		if (received != lengths[1]) {
			errno = EINVAL;
			return -1;
		}
	}

	received = lengths[0] + lengths[1];

	vk_vectoring_mark_received(ring_rx, received);

	return vk_size_return(ring_rx, received);
}

/* read into vector-ring from vector-ring */
ssize_t vk_vectoring_splice(struct vk_vectoring *ring_rx, struct vk_vectoring *ring_tx, ssize_t len) {
	ssize_t received;

	received = vk_vectoring_recv_splice(ring_rx, ring_tx, len);
	
	if (received < vk_vectoring_vector_rx_len(ring_rx)) {
		/* read request not fully satisfied */
		ring_rx->tx_blocked = 1;
	} else {
		/* read request  fully satisfied */
		ring_rx->tx_blocked = 0;
	}

	if (received < vk_vectoring_vector_tx_len(ring_tx)) {
		/* write request not fully satisfied */
		ring_tx->tx_blocked = 1;
	} else {
		/* write request fully satisifed */
		ring_tx->tx_blocked = 0;
	}

	/* forward EOF status from tx to rx */
	if (vk_vectoring_has_nodata(ring_tx)) { /* only when rx is drained */
		vk_vectoring_mark_eof(ring_rx);
	} else if ( ! vk_vectoring_has_eof(ring_tx)) {
		vk_vectoring_clear_eof(ring_rx);
	}

	return received;
}

/* produce EOF or error */
ssize_t vk_vectoring_hup(struct vk_vectoring *ring) {
    if (vk_vectoring_has_eof(ring)) {
        errno = EINVAL;
        return -1;
    } else {
        vk_vectoring_mark_eof(ring);
        return 1;
    }
}

/* consume EOF or error */
ssize_t vk_vectoring_readhup(struct vk_vectoring *ring) {
    if (vk_vectoring_has_eof(ring)) {
        vk_vectoring_clear_eof(ring);
        return 1;
    } else {
        errno = EINVAL;
        return -1;
    }
}