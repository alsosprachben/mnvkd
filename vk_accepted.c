#include "vk_accepted.h"
#include "vk_accepted_s.h"

#include <unistd.h>
#include <sys/socket.h>

size_t vk_accepted_alloc_size() {
    return sizeof (struct vk_accepted);
}
struct sockaddr *vk_accepted_get_client_address(struct vk_accepted *accepted_ptr) {
    return (struct sockaddr *) &accepted_ptr->client_address;
}
socklen_t vk_accepted_get_client_address_storage_len(struct vk_accepted *accepted_ptr) {
    return sizeof (accepted_ptr->client_address);
}
socklen_t vk_accepted_get_client_address_len(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->client_address_len;
}
void vk_accepted_set_client_address_len(struct vk_accepted *accepted_ptr, socklen_t client_address_len) {
    accepted_ptr->client_address_len = client_address_len;
}
socklen_t *vk_accepted_get_client_address_len_ptr(struct vk_accepted *accepted_ptr) {
    return &accepted_ptr->client_address_len;
}

char *vk_accepted_get_client_address_str(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->client_address_str;
}
const char *vk_accepted_set_client_address_str(struct vk_accepted *accepted_ptr) {
	return inet_ntop(vk_accepted_get_client_address(accepted_ptr)->sa_family, vk_accepted_get_client_address(accepted_ptr), vk_accepted_get_client_address_str(accepted_ptr), vk_accepted_get_client_address_strlen(accepted_ptr));
}
size_t vk_accepted_get_client_address_strlen(struct vk_accepted *accepted_ptr) {
    return sizeof (accepted_ptr->client_address_str);
}

struct that *vk_accepted_get_vk(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->vk_ptr;
}
void vk_accepted_set_vk(struct vk_accepted *accepted_ptr, struct that *that) {
    accepted_ptr->vk_ptr = that;
}

struct vk_proc *vk_accepted_get_proc(struct vk_accepted *accepted_ptr) {
    return accepted_ptr->proc_ptr;
}
void vk_accepted_set_proc(struct vk_accepted *accepted_ptr, struct vk_proc *proc_ptr) {
    accepted_ptr->proc_ptr = proc_ptr;
}
