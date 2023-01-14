#ifndef VK_VECTORING_S_H
#define VK_VECTORING_S_H

#include <unistd.h>
#include <sys/uio.h>

struct vk_vectoring {
	char  *buf_start;
	size_t buf_len;
	size_t tx_cursor;
	size_t tx_len;
	struct iovec vector_tx[2];
	struct iovec vector_rx[2];
	int    error; /* errno */
	int    eof;   /* end of file / hang up */
	int    rx_blocked; /* last read operation was not fully satisfied due to a physical block */
	int    tx_blocked; /* last write operation was not fully satisfied due to a physical block */
	int    effect; /* Transfer should cause effects to be applied to coroutine. */
};

#endif
