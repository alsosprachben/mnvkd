#ifndef VK_RFC_S_H
#define VK_RFC_S_H

struct vk_rfcchunkhead {
	size_t size;
	char head[19];	// 16 (size_t hex) + 2 (\r\n) + 1 (\0)
	char tail[3];	/* 2 (\r\n) + 1 (\0) */
};

#endif