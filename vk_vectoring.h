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

/* to initialize ring buffer around a buffer */
void vk_vectoring_init(struct vk_vectoring *ring, char *start, char *stop);
#define VK_VECTORING_INIT(ring, buf) vk_vectoring_init(ring, &(buf), sizeof (buf))

/* read from file-descriptor to vector-ring */
ssize_t vk_vectoring_read(struct vk_vectoring *ring, int d);
/* write to file-descriptor from vector-ring */
ssize_t vk_vectoring_write(struct vk_vectoring *ring, int d);

/* send from vector-ring to receive-buffer */
ssize_t vk_vectoring_recv(struct vk_vectoring *ring, void *buf, size_t len);
/* receive to vector-ring from send-buffer */
ssize_t vk_vectoring_send(struct vk_vectoring *ring, const void *buf, size_t len);


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
