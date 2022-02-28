#include "vk_rfc.h"

#include <strings.h>

/* from the return of vk_readline(), trim the newline, and adjust size */
void rtrim(char *line, int *size_ptr) {
	if (*size_ptr < 1) {
		return;
	}
	if (    line[*size_ptr - 1] == '\n') {
		line[*size_ptr - 1] = '\0';;
		--(*size_ptr);
	}
	if (*size_ptr < 1) {
		return;
	}
	if (    line[*size_ptr - 1] == '\r') {
		line[*size_ptr - 1] = '\0';
		--(*size_ptr);
	}
}

/* trim spaces from the left by returning the start of the string after the prefixed spaces */
char *ltrim(char *line, int *size_ptr) {
	while (*size_ptr > 0 && line[0] == ' ') {
		line++;
		size_ptr--;
	}
	/* dprintf(2, "rtrim() = %s\n", line); */
	return line;
}
char *ltrimlen(char *line) {
	int size = strlen(line);
	return ltrim(line, &size);
}

int parse_header(char *line, int *size_ptr, char **key_ptr, char **val_ptr) {
	char *tok;

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

/* branchless hex-char to int */
#define HEXVAL(b)        ((((b) & 0x1f) + (((b) >> 6) * 0x19) - 0x10) & 0xF)
size_t hex_size(char *val) {
	size_t size;
	int i;

	size = 0;
	for (i = 0; i < 16 && val[i] != '\0'; i++) {
		/* dprintf(2, "%i %c %u\n", i, val[i], HEXVAL(val[i])); */
		size <<= 4;
		size |= HEXVAL(val[i]); 
	}

	return size;
}
/* branchless dec-char to int */
#define DECVAL(b)        ((b) - 0x30)
size_t dec_size(char *val) {
	size_t size;
	int i;

	size = 0;
	for (i = 0; i < 20 && val[i] >= '0' && val[i] <= '9'; i++) {
		/* dprintf(2, "%i %c %u\n", i, val[i], DECVAL(val[i])); */
		size *= 10;
		size += DECVAL(val[i]); 
	}

	return size;
}
