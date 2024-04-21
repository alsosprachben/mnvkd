#ifndef VK_FETCH_S_H
#define VK_FETCH_S_H

#include <vk_socket.h>

/* Definition of the fetch structure */
struct vk_fetch {
	char method[10];
	char url[2048];
	struct {
		char key[256];
		char value[256];
	} headers[20];
	int header_count;
	int close;
};

#endif /* VK_FETCH_S_H */