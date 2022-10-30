/* Copyright 2022 BCW. All Rights Reserved. */
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>

#include "vk_heap.h"
#include "vk_heap_s.h"
#include "debug.h"

size_t vk_heap_alloc_size() {
	return sizeof (struct vk_heap);
}

int vk_heap_map(struct vk_heap *hd, void *addr, size_t len, int prot, int flags, int fd, off_t offset, int entered) {
	hd->prot_inside = prot | PROT_READ | PROT_WRITE;
	hd->prot_outside = prot;

	hd->mapping.addr   = addr;
	hd->mapping.len    = len;
	hd->mapping.prot   = entered ? hd->prot_inside : hd->prot_outside;
	hd->mapping.flags  = flags;
	hd->mapping.fd     = fd;
	hd->mapping.offset = offset;

	hd->mapping.retval = mmap(addr, len, hd->mapping.prot, flags, fd, offset);
	if (hd->mapping.retval == MAP_FAILED) {
		return -1;
	}

	hd->addr_start  = hd->mapping.retval;
	hd->addr_stop   = hd->addr_start + hd->mapping.len;
	hd->addr_cursor = hd->addr_start;

	return hd->mapping.len;
}

int vk_heap_unmap(struct vk_heap *hd) {
	return munmap(hd->mapping.retval, hd->mapping.len);
}

int vk_heap_enter(struct vk_heap *hd) {
	int rc;

	if (hd->mapping.prot != hd->prot_inside) {
		hd->mapping.prot = hd->prot_inside;

		rc = mprotect(hd->mapping.retval, hd->mapping.len, hd->mapping.prot);
		if (rc == -1) {
			return -1;
		}
	}

	return 0;
}

int vk_heap_exit(struct vk_heap *hd) {
	int rc;

	if (hd->mapping.prot != hd->prot_outside) {
		hd->mapping.prot = hd->prot_outside;

		rc = mprotect(hd->mapping.retval, hd->mapping.len, hd->mapping.prot);
		if (rc == -1) {
			return -1;
		}
	}

	return 0;
}

#define BLAH_VK_PAGESIZE 4096
size_t _vk_pagesize() {
	static long page_size;
	static int page_size_set = 0;
	if (! page_size_set) {
		page_size = sysconf(_SC_PAGE_SIZE);
		page_size_set = 1;
	}

	return (size_t) page_size;
}
size_t vk_pagesize() {
#ifdef VK_PAGESIZE
	return VK_PAGESIZE;
#else
	return _vk_pagesize();
#endif 
}

size_t vk_blocklen(size_t nmemb, size_t count) {
	size_t len;
	size_t blocklen;

	/* array of count, plus enough space for the linking len to pop to the start */
	len = nmemb * count + sizeof (size_t);
	blocklen = len / vk_pagesize();
	blocklen *= vk_pagesize();
	if (blocklen < len) {
		blocklen += vk_pagesize();
	}

	return blocklen;
}

void *vk_heap_push(struct vk_heap *hd, size_t nmemb, size_t count) {
	size_t len;
	void *addr;

	DBG("vk_heap_push(%zu, %zu)\n", nmemb, count);
	len = vk_blocklen(nmemb, count);

	if ((char *) hd->addr_cursor + len <= (char *) hd->addr_stop) {
		addr = hd->addr_cursor;

		/* push cursor and link */
		hd->addr_cursor = (char *) hd->addr_cursor + len;
		((size_t *) hd->addr_cursor)[-1] = len;

		DBG("heap use = %zu/%zu\n", (size_t) ((char *) hd->addr_cursor - (char *) hd->addr_start), (size_t) ((char *) hd->addr_stop - (char *) hd->addr_start));
		return addr;
	}
	
	DBG("vk_head_push() needs page count of %zu\n", ((size_t) ((char *) hd->addr_cursor + len - (char *) hd->addr_start)) / 4096);
	errno = ENOMEM;
	return NULL;
}

int vk_heap_pop(struct vk_heap *hd) {
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

void *vk_heap_get_start(struct vk_heap *hd) {
	return hd->addr_start;
}

void *vk_heap_get_cursor(struct vk_heap *hd) {
	return hd->addr_cursor;
}

void *vk_heap_get_stop(struct vk_heap *hd) {
	return hd->addr_stop;
}

size_t vk_heap_get_free(struct vk_heap *hd) {
	return hd->addr_stop - hd->addr_cursor;
}
