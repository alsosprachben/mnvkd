#ifndef VK_ACCEPTED_H
#define VK_ACCEPTED_H

#include <unistd.h>
#include <sys/socket.h>

struct vk_accepted;

size_t vk_accepted_alloc_size();

void vk_accepted_set_address(struct vk_accepted *accepted_ptr, struct sockaddr *address_ptr, socklen_t address_len);
struct sockaddr *vk_accepted_get_address(struct vk_accepted *accepted_ptr);
socklen_t vk_accepted_get_address_storage_len(struct vk_accepted *accepted_ptr);

socklen_t vk_accepted_get_address_len(struct vk_accepted *accepted_ptr);
void vk_accepted_set_address_len(struct vk_accepted *accepted_ptr, socklen_t address_len);
socklen_t *vk_accepted_get_address_len_ptr(struct vk_accepted *accepted_ptr);

char *vk_accepted_get_address_str(struct vk_accepted *accepted_ptr);
const char *vk_accepted_set_address_str(struct vk_accepted *accepted_ptr);
size_t vk_accepted_get_address_strlen(struct vk_accepted *accepted_ptr);

char *vk_accepted_get_port_str(struct vk_accepted *accepted_ptr);
int vk_accepted_set_port_str(struct vk_accepted *accepted_ptr);
size_t vk_accepted_get_port_strlen(struct vk_accepted *accepted_ptr);

struct vk_thread *vk_accepted_get_vk(struct vk_accepted *accepted_ptr);
void vk_accepted_set_vk(struct vk_accepted *accepted_ptr, struct vk_thread *that);

struct vk_proc *vk_accepted_get_proc(struct vk_accepted *accepted_ptr);
void vk_accepted_set_proc(struct vk_accepted *accepted_ptr, struct vk_proc *proc_ptr);

#endif