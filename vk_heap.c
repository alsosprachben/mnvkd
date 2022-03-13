#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>

#include "vk_heap.h"
#include "debug.h"

int vk_heap_map(struct vk_heap_descriptor *hd, void *addr, size_t len, int prot, int flags, int fd, off_t offset) {
	hd->mapping.addr   = addr;
	hd->mapping.len    = len;
	hd->mapping.prot   = prot;
	hd->mapping.flags  = flags;
	hd->mapping.fd     = fd;
	hd->mapping.offset = offset;

	hd->mapping.retval = mmap(addr, len, prot, flags, fd, offset);
	if (hd->mapping.retval == MAP_FAILED) {
		return -1;
	}

	hd->prot_inside = prot | PROT_READ | PROT_WRITE;
	hd->prot_outside = prot;

	hd->addr_start  = hd->mapping.retval;
	hd->addr_stop   = hd->addr_start + hd->mapping.len;
	hd->addr_cursor = hd->addr_start;

	return hd->mapping.len;
}

int vk_heap_unmap(struct vk_heap_descriptor *hd) {
	return munmap(hd->mapping.retval, hd->mapping.len);
}

int vk_heap_enter(struct vk_heap_descriptor *hd) {
	int rc;

	if (hd->mapping.flags != hd->prot_inside) {
		hd->mapping.flags = hd->prot_inside;

		rc = mprotect(hd->mapping.retval, hd->mapping.len, hd->mapping.flags);
		if (rc == -1) {
			return -1;
		}
	}

	return 0;
}

int vk_heap_exit(struct vk_heap_descriptor *hd) {
	int rc;

	if (hd->mapping.flags != hd->prot_outside) {
		hd->mapping.flags = hd->prot_outside;

		rc = mprotect(hd->mapping.retval, hd->mapping.len, hd->mapping.flags);
		if (rc == -1) {
			return -1;
		}
	}

	return 0;
}

size_t calloc_blocklen(size_t nmemb, size_t count) {
	size_t len;
	size_t blocklen;

	/* array of count, plus enough space for the linking len to pop to the start */
	len = nmemb * count + sizeof (size_t);
	blocklen = len / 4096;
	blocklen *= 4096;
	if (blocklen < len) {
		blocklen += 4096;
	}

	return blocklen;
}

void *vk_heap_push(struct vk_heap_descriptor *hd, size_t nmemb, size_t count) {
	size_t len;
	void *addr;

	DBG(2, "vk_heap_push(%zu, %zu)\n", nmemb, count);
	len = calloc_blocklen(nmemb, count);

	if ((char *) hd->addr_cursor + len <= (char *) hd->addr_stop) {
		addr = hd->addr_cursor;

		/* push cursor and link */
		hd->addr_cursor = (char *) hd->addr_cursor + len;
		((size_t *) hd->addr_cursor)[-1] = len;

		DBG(2, "heap use = %zu/%zu\n", (size_t) ((char *) hd->addr_cursor - (char *) hd->addr_start), (size_t) ((char *) hd->addr_stop - (char *) hd->addr_start));
		return addr;
	}
	
	DBG(2, "vk_head_push() needs page count of %zu\n", ((size_t) ((char *) hd->addr_cursor + len - (char *) hd->addr_start)) / 4096);
	errno = ENOMEM;
	return NULL;
}

int vk_heap_pop(struct vk_heap_descriptor *hd) {
	size_t len;
	size_t *len_ptr;

	len_ptr = (size_t *) hd->addr_cursor - 1;
	if (len_ptr < (size_t *) hd->addr_start) {
		errno = ENOENT;
		return -1;
	} else if (len_ptr >= (size_t *) hd->addr_stop) {
		errno = EINVAL;
		return -1;
	}

	/* pop cursor from link */
	len = ((size_t *) hd->addr_cursor)[-1];
	hd->addr_cursor = (char *) hd->addr_cursor - len;

	return 0;
}

