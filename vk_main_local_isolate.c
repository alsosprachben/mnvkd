#include <stdlib.h>

#include "vk_main_local_isolate.h"

#include "vk_heap.h"
#include "vk_proc_local.h"
#include "vk_proc_local_s.h"
#include "vk_stack.h"


int vk_main_local_isolate_init(vk_isolate_t *iso,
                               vk_func main_vk,
                               void *arg_buf,
                               size_t arg_len,
                               size_t page_count)
{
    int rc = 0;
    struct vk_heap *heap_ptr = NULL;
    struct vk_proc_local *proc_local_ptr = NULL;
    struct vk_thread *that = NULL;

    do { /* for cleanup break */
        heap_ptr = calloc(1, vk_heap_alloc_size());
        if (!heap_ptr) { perror("calloc"); rc = -1; break; }

        rc = vk_heap_map(heap_ptr, NULL, page_count * 4096, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0, 1);
        if (rc == -1) { perror("vk_heap_map"); break; }

        proc_local_ptr = vk_stack_push(vk_heap_get_stack(heap_ptr), 1, vk_proc_local_alloc_size());
        if (!proc_local_ptr) { perror("vk_stack_push"); rc = -1; break; }
        vk_proc_local_init(proc_local_ptr);
        vk_proc_local_set_proc_id(proc_local_ptr, 1);
        vk_proc_local_set_stack(proc_local_ptr, vk_heap_get_stack(heap_ptr));

        that = vk_stack_push(vk_proc_local_get_stack(proc_local_ptr), 1, vk_alloc_size());
        if (!that) { perror("vk_stack_push"); rc = -1; break; }

        /* Bind stdin/stdout (blocking is fine for local runner) */
        VK_INIT(that, proc_local_ptr, main_vk, 0, VK_FD_TYPE_SOCKET_STREAM, 1, VK_FD_TYPE_SOCKET_STREAM);
        rc = vk_copy_arg(that, arg_buf, arg_len);
        if (rc == -1) { perror("vk_copy_arg"); break; }

        vk_proc_local_enqueue_run(proc_local_ptr, that);

        rc = vk_proc_local_execute_isolated(proc_local_ptr, iso);
    } while (0);

    if (heap_ptr) free(heap_ptr);
    return rc;
}
