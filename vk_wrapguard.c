#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "vk_wrapguard.h"

#define BLAH_VK_PAGESIZE 4096
size_t vk__pagesize()
{
	static long page_size;
	static int page_size_set = 0;
	if (!page_size_set) {
		page_size = sysconf(_SC_PAGE_SIZE);
		page_size_set = 1;
	}

	return (size_t)page_size;
}
size_t vk_pagesize()
{
#ifdef VK_PAGESIZE
	return VK_PAGESIZE;
#else
	return vk__pagesize();
#endif
}

/* BEGIN from OpenBSD (adapted from reallocarray.c) */
/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 &lt;= SIZE_MAX
 * if both s1 &lt; MUL_NO_OVERFLOW and s2 &lt; MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW (1UL << (sizeof(size_t) * 4))

int vk_wrapguard_mult(size_t a, size_t b)
{
	if ((a >= MUL_NO_OVERFLOW || b >= MUL_NO_OVERFLOW) && a > 0 && SIZE_MAX / a < b) {
		errno = ERANGE;
		return -1;
	}
	return 0;
}
/* END from OpenBSD */

int vk_wrapguard_add(size_t a, size_t b)
{
	if (a + b < a || a + b < b) {
		errno = ERANGE;
		return -1;
	}
	return 0;
}

int vk_safe_mult(size_t a, size_t b, size_t* c_ptr)
{
	int rc;

	rc = vk_wrapguard_mult(a, b);
	if (rc == -1) {
		return -1;
	}

	*c_ptr = a * b;

	return 0;
}

int vk_safe_add(size_t a, size_t b, size_t* c_ptr)
{
	int rc;

	rc = vk_wrapguard_add(a, b);
	if (rc == -1) {
		return -1;
	}

	*c_ptr = a + b;

	return 0;
}

int vk_safe_pagelen(size_t nmemb, size_t count, size_t* pagelen_ptr)
{
	int rc;
	size_t len;
	size_t pagelen;

	rc = vk_safe_mult(nmemb, count, &len);
	if (rc == -1) {
		return -1;
	}

	pagelen = len / vk_pagesize();

	if (pagelen * vk_pagesize() < len) {
		++pagelen;
	}

	*pagelen_ptr = pagelen;

	return 0;
}

#define vk_hugepagesize (2 * 1024 * 1024)
int vk_safe_hugepagelen(size_t nmemb, size_t count, size_t* pagelen_ptr)
{
	int rc;
	size_t len;
	size_t pagelen;

	rc = vk_safe_mult(nmemb, count, &len);
	if (rc == -1) {
		return -1;
	}

	pagelen = len / vk_hugepagesize;

	if (pagelen * vk_hugepagesize < len) {
		++pagelen;
	}

	*pagelen_ptr = pagelen;

	return 0;
}

int vk_safe_alignedlen(size_t nmemb, size_t count, size_t* alignedlen_ptr)
{
	int rc;
	size_t pagelen;

	rc = vk_safe_pagelen(nmemb, count, &pagelen);
	if (rc == -1) {
		return -1;
	}

	rc = vk_safe_mult(pagelen, vk_pagesize(), alignedlen_ptr);
	if (rc == -1) {
		return -1;
	}

	return 0;
}

int vk_safe_hugealignedlen(size_t nmemb, size_t count, size_t* alignedlen_ptr)
{
	int rc;
	size_t pagelen;

	rc = vk_safe_hugepagelen(nmemb, count, &pagelen);
	if (rc == -1) {
		return -1;
	}

	rc = vk_safe_mult(pagelen, vk_hugepagesize, alignedlen_ptr);
	if (rc == -1) {
		return -1;
	}

	return 0;
}
