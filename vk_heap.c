/* Copyright 2022 BCW. All Rights Reserved. */
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>

#include "vk_heap.h"
#include "vk_heap_s.h"
#include "vk_stack.h"
#include "vk_stack_s.h"
#include "vk_debug.h"

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

	vk_stack_init(&hd->stack, hd->mapping.retval, hd->mapping.len);

	return hd->mapping.len;
}

struct vk_stack *vk_heap_get_stack(struct vk_heap *hd) {
	return &hd->stack;
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
