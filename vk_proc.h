#ifndef VK_PROC_H
#define VK_PROC_H

#include <sys/types.h>

struct vk_proc;

int vk_proc_init(struct vk_proc *proc_ptr, void *map_addr, size_t map_len, int map_prot, int map_flags, int map_fd, off_t map_offset);
int vk_proc_deinit(struct vk_proc *proc_ptr);

#define VK_PROC_INIT_PRIVATE(proc_ptr, map_len) vk_proc_init(proc_ptr, NULL, map_len, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0)
#define VK_PROC_INIT_PUBLIC( proc_ptr, map_len) vk_proc_init(proc_ptr, NULL, map_len, PROT_NONE,            MAP_ANON|MAP_PRIVATE, -1, 0)

#endif