#ifndef VK_RFC_H
#define VK_RFC_H

#include "vk_thread.h"

/*
 * Interfaces for vk_rfc.c functions
 */

/*
 * Utilities for Parsing
 */

/* from the return of vk_readline(), trim the newline, and adjust size */
void rtrim(char *line, ssize_t *size_ptr);

/* trim spaces from the left by returning the start of the string after the prefixed spaces */
char *ltrim(char *line, ssize_t *size_ptr);
char *ltrimlen(char *line);

int parse_header(char *line, ssize_t *size_ptr, char **key_ptr, char **val_ptr);

/* From the BSD man page for strncpy */
#define copy_into(buf, input) do { \
	(void)strncpy(buf, input, sizeof (buf)); \
	buf[sizeof(buf) - 1] = '\0'; \
} while (0)

/* int to hex */
size_t size_hex(char *val, size_t len, size_t size);

/* hex to int */
size_t hex_size(const char *val);

/* dec to int */
size_t dec_size(const char *val);

/*
 * `struct vk_rfcchunk` interface
 */
struct vk_rfcchunk;
char  *vk_rfcchunk_get_buf(      struct vk_rfcchunk *chunk);
size_t vk_rfcchunk_get_buf_size( struct vk_rfcchunk *chunk);
size_t vk_rfcchunk_get_size(     struct vk_rfcchunk *chunk);
void   vk_rfcchunk_set_size(     struct vk_rfcchunk *chunk, size_t size);
void   vk_rfcchunk_update_size(  struct vk_rfcchunk *chunk);
char  *vk_rfcchunk_get_head(     struct vk_rfcchunk *chunk);
size_t vk_rfcchunk_get_head_size(struct vk_rfcchunk *chunk);
size_t vk_rfcchunk_update_head(  struct vk_rfcchunk *chunk);
char  *vk_rfcchunk_get_tail(     struct vk_rfcchunk *chunk);
size_t vk_rfcchunk_get_tail_size(struct vk_rfcchunk *chunk);

/*
 * Blocking macros to extend `vk_thread.h` blocking macros
 */

/*
 * For reading and writing RFC headers
 */
#define vk_socket_readrfcline(rc_arg, socket_ptr, buf_arg, len_arg) do { \
	vk_socket_readline((rc_arg), (socket_ptr), (buf_arg), (len_arg)); \
	if ((rc_arg) == 0) { \
		vk_raise(EPIPE); \
	} \
	rtrim((buf_arg), &(rc_arg)); \
} while (0)

#define vk_socket_readrfcheader(rc_arg, socket_ptr, buf_arg, len_arg, key_ptr, val_ptr) do { \
	vk_socket_readrfcline((rc_arg), (socket_ptr), (buf_arg), (len_arg)); \
	(rc_arg) = parse_header((buf_arg), &(rc_arg), (key_ptr), (val_ptr)); \
} while (0)

#define vk_readrfcline(  rc_arg, buf_arg, len_arg)                   vk_socket_readrfcline(  rc_arg, vk_get_socket(that), buf_arg, len_arg)
#define vk_readrfcheader(rc_arg, buf_arg, len_arg, key_ptr, val_ptr) vk_socket_readrfcheader(rc_arg, vk_get_socket(that), buf_arg, len_arg, key_ptr, val_ptr)

#define vk_socket_writerfcheader(socket_ptr, key_arg, key_len, val_arg, val_len) do { \
    vk_socket_write(socket_ptr, (key_arg), (key_len)); \
    vk_socket_write(socket_ptr, ": ", 2); \
    vk_socket_write(socket_ptr, (val_arg), (val_len)); \
    vk_socket_write(socket_ptr, "\r\n", 2); \
} while (0)
#define vk_socket_writerfcheaderend(socket_ptr) vk_socket_write(socket_ptr, "\r\n", 2)

#define vk_writerfcheader(key_arg, key_len, val_arg, val_len) vk_socket_writerfcheader(vk_get_socket(that), (key_arg), (key_len), (val_arg), (val_len))
#define vk_writerfcheaderend() vk_socket_writerfcheaderend(vk_get_socket(that))

/*
 * For reading and writing HTTP chunks
 */
#define vk_socket_readrfcchunk(rc_arg, socket_ptr, chunk_arg) do {                   \
	vk_socket_read(rc_arg, (socket_ptr), vk_rfcchunk_get_buf(chunk_arg), vk_rfcchunk_get_buf_size(chunk_arg)); \
	if ((rc_arg) != vk_rfcchunk_get_buf_size(chunk_arg)) {                                                             \
        vk_raise(EPIPE);                                                            \
    }                                                                               \
	vk_rfcchunk_set_size((chunk_arg), (size_t) (rc_arg));                           \
} while (0)
#define vk_socket_writerfcchunk(socket_ptr, chunk_arg) do { \
    vk_socket_write(socket_ptr, vk_rfcchunk_get_buf(chunk_arg), vk_rfcchunk_get_size(chunk_arg)); \
} while (0)

#define vk_socket_writerfcchunk_proto(rc_arg, socket_ptr, chunk_arg) do { \
    rc_arg = vk_rfcchunk_update_head((chunk_arg)); \
	vk_socket_write(socket_ptr, vk_rfcchunk_get_head(chunk_arg), rc_arg); \
	vk_socket_write(socket_ptr, vk_rfcchunk_get_buf(chunk_arg), vk_rfcchunk_get_size(chunk_arg)); \
	vk_socket_write(socket_ptr, "\r\n", 2); \
	rc_arg += (int) vk_rfcchunk_get_size(chunk_arg); \
} while (0)

#define vk_socket_readrfcchunk_proto(rc_arg, socket_ptr, chunk_arg) do { \
	vk_socket_readrfcline((rc_arg), socket_ptr, vk_rfcchunk_get_head(chunk_arg), vk_rfcchunk_get_head_size(chunk_arg) - 1); \
	if ((rc_arg) <= 0 || (rc_arg) > vk_rfcchunk_get_head_size(chunk_arg) - 1) { \
		vk_raise(EPROTO); \
	} \
	vk_rfcchunk_get_head(chunk_arg)[(rc_arg)] = '\0'; \
	vk_rfcchunk_update_size(chunk_arg); \
	if (vk_rfcchunk_get_size(chunk_arg) == 0) { \
		rc_arg = 0; /* terminating chunk */ \
		break; \
	} \
	if (vk_rfcchunk_get_size(chunk_arg) > vk_rfcchunk_get_buf_size(chunk_arg)) { \
        vk_raise(ENOBUFS); \
    } \
	vk_socket_read((rc_arg), (socket_ptr), vk_rfcchunk_get_buf(chunk_arg), vk_rfcchunk_get_size(chunk_arg)); \
	if (rc_arg != vk_rfcchunk_get_size(chunk_arg)) { \
	    vk_raise(EPIPE); \
	} \
	vk_rfcchunk_set_size((chunk_arg), (size_t) (rc_arg));                           \
	if (vk_rfcchunk_get_size(chunk_arg) > 0) {                                           \
        vk_socket_read((rc_arg), (socket_ptr), vk_rfcchunk_get_tail(chunk_arg), 2);       \
        if ((rc_arg) != 2) {                                                             \
            vk_raise(EPIPE);                                                             \
        }                                                                                \
        vk_rfcchunk_get_tail(chunk_arg)[2] = '\0';                                       \
        if (vk_rfcchunk_get_tail(chunk_arg)[0] != '\r' || vk_rfcchunk_get_tail(chunk_arg)[1] != '\n') { \
            vk_raise(EPROTO);                                                              \
        }                                                                                \
    }                                                                                    \
    (rc_arg) = (int) vk_rfcchunk_get_size(chunk_arg);                                    \
} while (0)

#define vk_readrfcchunk(       rc_arg, chunk_arg) vk_socket_readrfcchunk(       rc_arg, vk_get_socket(that), chunk_arg)
#define vk_writerfcchunk(              chunk_arg) vk_socket_writerfcchunk(              vk_get_socket(that), chunk_arg)

#define vk_writerfcchunk_proto(rc_arg, chunk_arg) vk_socket_writerfcchunk_proto(rc_arg, vk_get_socket(that), chunk_arg)
#define vk_writerfcchunkend_proto()               vk_socket_write_literal(              vk_get_socket(that), "0\r\n\r\n")
#define vk_readrfcchunk_proto( rc_arg, chunk_arg) vk_socket_readrfcchunk_proto( rc_arg, vk_get_socket(that), chunk_arg)

#endif
