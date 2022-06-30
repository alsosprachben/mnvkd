#ifndef VK_RFC_H
#define VK_RFC_H

#include "vk_thread.h"

/* from the return of vk_readline(), trim the newline, and adjust size */
void rtrim(char *line, int *size_ptr);

/* trim spaces from the left by returning the start of the string after the prefixed spaces */
char *ltrim(char *line, int *size_ptr);
char *ltrimlen(char *line);

int parse_header(char *line, int *size_ptr, char **key_ptr, char **val_ptr);

/* From the BSD man page for strncpy */
#define copy_into(buf, input) { \
	(void)strncpy(buf, input, sizeof (buf)); \
	buf[sizeof(buf) - 1] = '\0'; \
}

/* int to hex */
size_t size_hex(char *val, size_t len, size_t size);

/* hex to int */
size_t hex_size(char *val);

/* dec to int */
size_t dec_size(char *val);


#define vk_socket_readrfcline(rc_arg, socket_ptr, buf_arg, len_arg) do { \
	vk_socket_readline((rc_arg), (socket_ptr), (buf_arg), (len_arg)); \
	if ((rc_arg) == 0) { \
		break; \
	} \
	rtrim((buf_arg), &(rc_arg)); \
} while (0)

#define vk_socket_readrfcheader(rc_arg, socket_ptr, buf_arg, len_arg, key_ptr, val_ptr) do { \
	vk_socket_readrfcline((rc_arg), (socket_ptr), (buf_arg), (len_arg)); \
	(rc_arg) = parse_header((buf_arg), &(rc_arg), (key_ptr), (val_ptr)); \
} while (0)

#define vk_readrfcline(  rc_arg, buf_arg, len_arg)                   vk_socket_readrfcline(  rc_arg, vk_get_socket(that), buf_arg, len_arg)
#define vk_readrfcheader(rc_arg, buf_arg, len_arg, key_ptr, val_ptr) vk_socket_readrfcheader(rc_arg, vk_get_socket(that), buf_arg, len_arg, key_ptr, val_ptr)

struct rfcchunk {
	char   buf[4096];
	size_t size;
	char   head[19]; // 16 (size_t hex) + 2 (\r\n) + 1 (\0)
};

#define vk_socket_readrfcchunk(rc_arg, socket_ptr, chunk_arg) do {                   \
	vk_socket_read(rc_arg, (socket_ptr), (chunk_arg).buf, sizeof ((chunk_arg).buf)); \
	(chunk_arg).size = (size_t) (rc_arg);                       \
} while (0)

#define vk_socket_writerfcchunk_proto(rc_arg, socket_ptr, chunk_arg) do { \
	rc_arg = size_hex((chunk_arg).head, sizeof ((chunk_arg).head) - 1, (chunk_arg).size); \
	(chunk_arg).head[rc++] = '\r'; \
	(chunk_arg).head[rc++] = '\n'; \
	vk_socket_write(socket_ptr, (chunk_arg).head, rc); \
	vk_socket_write(socket_ptr, (chunk_arg).buf, (chunk_arg).size); \
} while (0)

#define vk_socket_readrfcchunk_proto(rc_arg, socket_ptr, chunk_arg) do { \
	vk_socket_readrfcline(rc_arg, socket_ptr, (chunk_arg).head, sizeof ((chunk_arg).head) - 1); \
	if (rc_arg == 0) { \
		break; \
	} \
	(chunk_arg).head[rc_arg] = '\0'; \
	if (rc_arg == 0) { \
		break; \
	} \
	(chunk_arg).size = hex_size((chunk_arg).head); \
	if ((chunk_arg).size == 0) { \
		rc_arg = (int) (chunk_arg).size; \
		break; \
	} \
	vk_read(rc_arg, (chunk_arg).buf, (chunk_arg).size); \
	if (rc_arg != (chunk_arg).size) { \
		vk_raise(EPIPE); \
	} \
} while (0)

#define vk_readrfcchunk(rc_arg, chunk_arg) vk_socket_readrfcchunk(rc_arg, vk_get_socket(that), chunk_arg)
#define vk_writerfcchunk_proto(rc_arg, chunk_arg) vk_socket_writerfcchunk_proto(rc_arg, vk_get_socket(that), chunk_arg)
#define vk_readrfcchunk_proto(rc_arg, chunk_arg) vk_socket_readrfcchunk_proto(rc_arg, vk_get_socket(that), chunk_arg)

#endif
