#ifndef VK_VECTORING_S_H
#define VK_VECTORING_S_H

#include <sys/uio.h>
#include <unistd.h>

struct vk_vectoring {
	char* buf_start;
	size_t buf_len;
	size_t tx_cursor;
	size_t tx_len;
	struct iovec vector_tx[2];
	struct iovec vector_rx[2];
	int error;	/* `ferror()` and `clearerr()` -- errno value from last operation */
	int eof;	/* `feof()` and `clearerr()` -- end of file / hang up -- can be reset */
	int closed;	/* `fclose()` and `shutdown()` -- transport is closed -- cannot be reset */
	int rx_blocked; /* last read operation was not fully satisfied due to a physical block */
	int tx_blocked; /* last write operation was not fully satisfied due to a physical block */
	int effect;	/* Transfer should cause effects to be applied to coroutine. */
};

#endif
