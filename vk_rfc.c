/* Copyright 2022 BCW. All Rights Reserved. */
#include <strings.h>

#include "vk_debug.h"
#include "vk_rfc.h"
#include "vk_rfc_s.h"

/* from the return of vk_readline(), trim the newline, and adjust size */
void rtrim(char* line, ssize_t* size_ptr)
{
	if (*size_ptr < 1) {
		return;
	}
	if (line[*size_ptr - 1] == '\n') {
		line[*size_ptr - 1] = '\0';
		--(*size_ptr);
	}
	if (*size_ptr < 1) {
		return;
	}
	if (line[*size_ptr - 1] == '\r') {
		line[*size_ptr - 1] = '\0';
		--(*size_ptr);
	}
}

/* trim spaces from the left by returning the start of the string after the prefixed spaces */
char* ltrim(char* line, ssize_t* size_ptr)
{
	if (*size_ptr < 0) {
		return line;
	}
	while (*size_ptr > 0 && line[0] == ' ') {
		line++;
		size_ptr--;
	}
	/* dprintf(2, "rtrim() = %s\n", line); */
	return line;
}
char* ltrimlen(char* line)
{
	ssize_t size = (ssize_t)strlen(line);
	return ltrim(line, &size);
}

int parse_header(char* line, ssize_t* size_ptr, char** key_ptr, char** val_ptr)
{
	char* tok;

	*key_ptr = NULL;
	*val_ptr = NULL;

	rtrim(line, size_ptr);
	line[*size_ptr] = '\0';
	if (*size_ptr == 0) {
		/* end of headers */
		return 0;
	}

	tok = line;
	*key_ptr = strsep(&tok, ": \t");
	if (*key_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (tok != NULL) {
		*val_ptr = ltrimlen(tok);
	}

	if (*val_ptr == NULL) {
		return 0;
	}

	return 1;
}

#ifndef flsl
/* From FreeBSD */
/*
 * Find Last Set bit
 */
int flsl(long mask)
{
	int bit;

	if (mask == 0)
		return (0);
	for (bit = 1; mask != 1; bit++)
		mask = (long)((unsigned long)mask >> 1);
	return (bit);
}
#endif

/* branchless int to hex-char */
#define VALHEX(v) ((((v) + 48) & (-((((v)-10) & 0x80) >> 7))) | (((v) + 55) & (-(((9 - (v)) & 0x80) >> 7))))
/* branchless find-last-set (hex) using first-last-set (bit) */
#define flsh(size) (flsl((long int)(size << 3)) >> 2)

/*
 * val: buffer into which to write
 * len: size remaining in buffer (must be >= 16 for highest 64-bit size)
 * size: the size to write in hex
 * returns: bytes written to val (not null-terminated)
 */
size_t size_hex(char* val, size_t len, size_t size)
{
	size_t i; // bytes processed
	size_t j; // copy pass
	char c;	  // character value;
	char h;	  // character hex ASCII value;

	len = len > 16 ? 16 : len; // if (len > 16) len = 16;
	DBG("flsl(%li) = %i\n", (long int)size, flsl((long int)size));
	DBG("flsh(%zu) = %i\n", size, flsh(size));
	DBG("flsh(0x0) = %i\n", flsh(0x0l));
	DBG("flsh(0x1) = %i\n", flsh(0x1l));
	DBG("flsh(0x7) = %i\n", flsh(0x7l));
	DBG("flsh(0xf) = %i\n", flsh(0xfl));
	DBG("flsh(0x1f) = %i\n", flsh(0x1fl));
	DBG("flsh(0x7f) = %i\n", flsh(0x7fl));
	DBG("flsh(0xff) = %i\n", flsh(0xffl));
	for (i = 16 - flsh(size), j = 0; j < len && i < 16; i++) {
		// branchless inner loop
		c = (char)((size >> (4 * (15 - i))) & 0xf);
		h = VALHEX(c);
		val[j] = h;
		DBG("i = %zu; shift = %zu; c = %i; h = %c; j = %zu;\n", i, 4 * (15 - i), (int)c, h, j);
		j += (j | c) > 0; // If a non-zero character has already been written, or the current value is non-zero,
				  // increment the write cursor.
	}
	j += j == 0; // if (j == 0) j++ // if only a zero has been written, j will still be 0

	return j; // index was incremented to a length after the last use
}

/* branchless hex-char to int */
#define HEXVAL(b) ((((b)&0x1f) + (((b) >> 6) * 0x19) - 0x10) & 0xF)
size_t hex_size(const char* val)
{
	size_t size;
	int i;

	size = 0;
	for (i = 0; i < 16 && val[i] != '\0'; i++) {
		// branchless inner loop
		/* dprintf(2, "%i %c %u\n", i, val[i], HEXVAL(val[i])); */
		size <<= 4;
		size |= HEXVAL(val[i]);
	}

	return size;
}
/* branchless dec-char to int */
#define DECVAL(b) ((b)-0x30)
size_t dec_size(const char* val)
{
	size_t size;
	int i;

	size = 0;
	for (i = 0; i < 20 && val[i] >= '0' && val[i] <= '9'; i++) {
		// branchless inner loop
		/* dprintf(2, "%i %c %u\n", i, val[i], DECVAL(val[i])); */
		size *= 10;
		size += DECVAL(val[i]);
	}

	return size;
}


size_t vk_rfcchunkhead_get_size(struct vk_rfcchunkhead* chunkhead) { return chunkhead->size; }
void vk_rfcchunkhead_set_size(struct vk_rfcchunkhead* chunkhead, size_t size) { chunkhead->size = size; }
void vk_rfcchunkhead_update_size(struct vk_rfcchunkhead* chunkhead) { chunkhead->size = hex_size(chunkhead->head); }
char* vk_rfcchunkhead_get_head(struct vk_rfcchunkhead* chunkhead) { return chunkhead->head; }
size_t vk_rfcchunkhead_get_head_size(struct vk_rfcchunkhead* chunkhead) { return sizeof(chunkhead->head) - 1; }
size_t vk_rfcchunkhead_update_head(struct vk_rfcchunkhead* chunkhead)
{
	size_t rc;
	rc = size_hex((char*)&chunkhead->head, sizeof(chunkhead->head) - 1, chunkhead->size);
	chunkhead->head[rc++] = '\r';
	chunkhead->head[rc++] = '\n';
	chunkhead->head[rc] = '\0';
	return rc;
}
char* vk_rfcchunkhead_get_tail(struct vk_rfcchunkhead* chunkhead) { return chunkhead->tail; }
size_t vk_rfcchunkhead_get_tail_size(struct vk_rfcchunkhead* chunkhead) { return sizeof(chunkhead->tail) - 1; }
