#include "vk_client.h"
#include "vk_client_s.h"
#include "vk_kern.h"
#include "vk_heap.h"
#include "vk_proc.h"
#include "vk_thread.h"
#include "vk_connection.h"
#include <fcntl.h>
#include <stdlib.h>

size_t vk_client_alloc_size() { return sizeof(struct vk_client); }

const char* vk_client_get_address(struct vk_client* client_ptr) { return client_ptr->address; }
void vk_client_set_address(struct vk_client* client_ptr, const char* address) { client_ptr->address = address; }
const char* vk_client_get_port(struct vk_client* client_ptr) { return client_ptr->port; }
void vk_client_set_port(struct vk_client* client_ptr, const char* port) { client_ptr->port = port; }
vk_func vk_client_get_vk_func(struct vk_client* client_ptr) { return client_ptr->vk_func; }
void vk_client_set_vk_func(struct vk_client* client_ptr, vk_func func) { client_ptr->vk_func = func; }

int vk_client_init(struct vk_client* client_ptr, size_t page_count, int privileged, int isolated)
{
    int rc;
    struct vk_heap* kern_heap_ptr;
    struct vk_kern* kern_ptr;
    struct vk_proc* proc_ptr;
    struct vk_thread* vk_ptr;

    kern_heap_ptr = calloc(1, vk_heap_alloc_size());
    kern_ptr = vk_kern_alloc(kern_heap_ptr);
    if (!kern_ptr) {
        return -1;
    }

    proc_ptr = vk_kern_alloc_proc(kern_ptr, NULL);
    if (!proc_ptr) {
        return -1;
    }
    rc = VK_PROC_INIT_PRIVATE(proc_ptr, 4096 * page_count, 0, 1);
    if (rc == -1) {
        return -1;
    }
    vk_proc_set_privileged(proc_ptr, privileged);
    vk_proc_set_isolated(proc_ptr, isolated);

    vk_ptr = vk_proc_alloc_thread(proc_ptr);
    if (!vk_ptr) {
        return -1;
    }

    fcntl(0, F_SETFL, O_NONBLOCK);
    fcntl(1, F_SETFL, O_NONBLOCK);

    VK_INIT(vk_ptr, vk_proc_get_local(proc_ptr), vk_connection_client, 0, VK_FD_TYPE_SOCKET_STREAM, 1, VK_FD_TYPE_SOCKET_STREAM);
    rc = vk_copy_arg(vk_ptr, client_ptr, vk_client_alloc_size());
    if (rc == -1) {
        return -1;
    }

    vk_proc_local_enqueue_run(vk_proc_get_local(proc_ptr), vk_ptr);
    vk_kern_flush_proc_queues(kern_ptr, proc_ptr);
    rc = vk_kern_loop(kern_ptr);
    if (rc == -1) {
        return -1;
    }
    rc = vk_proc_free(proc_ptr);
    if (rc == -1) {
        return -1;
    }
    vk_kern_free_proc(kern_ptr, proc_ptr);
    return 0;
}

