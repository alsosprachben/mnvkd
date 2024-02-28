#ifndef VK_RFC_S_H
#define VK_RFC_S_H

struct vk_rfcchunk {
	char   buf[4096]; /* one page */
	size_t size; /* size of buf */
	char   head[19]; // 16 (size_t hex) + 2 (\r\n) + 1 (\0)
	char   tail[3]; /* 2 (\r\n) + 1 (\0) */
};

#endif