#ifndef VK_IO_OP_E_H
#define VK_IO_OP_E_H

enum vk_io_flag {
	VK_IO_F_NONE = 0,
	VK_IO_F_SOCKET,
	VK_IO_F_STREAM,
	VK_IO_F_ALLOW_PARTIAL,
	VK_IO_F_DIR_RX,
	VK_IO_F_DIR_TX,
};

#endif
