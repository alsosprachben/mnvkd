#ifndef VK_RFC_H
#define VK_RFC_H

#include "vk_state.h"

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


#define vk_socket_readrfcline(rc_arg, socket_arg, buf_arg, len_arg) do { \
	vk_socket_readline((rc_arg), (socket_arg), (buf_arg), (len_arg)); \
	if ((rc_arg) == 0) { \
		break; \
	} \
	rtrim((buf_arg), &(rc_arg)); \
} while (0)

#define vk_socket_readrfcheader(rc_arg, socket_arg, buf_arg, len_arg, key_ptr, val_ptr) do { \
	vk_socket_readrfcline((rc_arg), (socket_arg), (buf_arg), (len_arg)); \
	(rc_arg) = parse_header((buf_arg), &(rc_arg), (key_ptr), (val_ptr)); \
} while (0)

#define vk_readrfcline(  rc_arg, buf_arg, len_arg)                   vk_socket_readrfcline(  rc_arg, that->socket, buf_arg, len_arg)
#define vk_readrfcheader(rc_arg, buf_arg, len_arg, key_ptr, val_ptr) vk_socket_readrfcheader(rc_arg, that->socket, buf_arg, len_arg, key_ptr, val_ptr)
#endif
