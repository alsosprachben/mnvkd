#ifndef VK_DEBUG_H
#define VK_DEBUG_H

#include <stdio.h>

#ifndef DEBUG
#define DEBUG_COND 0
#else
#define DEBUG_COND 1
#endif

#define ERR(...) dprintf(2, __VA_ARGS__)
#define DBG(...) if (DEBUG_COND) ERR(__VA_ARGS__)

#define PRloc "%s:%i"
#define ARGloc __FILE__, __LINE__

#define PRvk "%s()[%s:%i]"
#include "vk_thread.h"
#define ARGvk(that) vk_get_func_name(that), vk_get_file(that), vk_get_line(that)

#define PRprocl "{%zu}"
#include "vk_proc_local.h"
#define ARGprocl(proc_local_ptr) vk_proc_local_get_proc_id(proc_local_ptr)

#define PRsocket ""
#include "vk_socket.h"
#define ARGsocket(socket_ptr) 

#define vk_logf(fmt, ...) ERR(PRloc " " PRprocl " " PRvk " " fmt, ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__) 
#define vk_dbgf(fmt, ...) DBG(PRloc " " PRprocl " " PRvk " " fmt, ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__) 
#define vk_log(fmt)       ERR(PRloc " " PRprocl " " PRvk " " fmt, ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that)) 
#define vk_dbg(fmt)       DBG(PRloc " " PRprocl " " PRvk " " fmt, ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that)) 
#define vk_perror(string) vk_logf("%s: %s\n", string, strerror(errno))

#define vk_proc_local_logf(fmt, ...) ERR(PRloc " " PRprocl " " fmt, ARGloc, ARGprocl(proc_local_ptr), __VA_ARGS__)
#define vk_proc_local_dbgf(fmt, ...) DBG(PRloc " " PRprocl " " fmt, ARGloc, ARGprocl(proc_local_ptr), __VA_ARGS__)
#define vk_proc_local_log(fmt)      ERR(PRloc " " PRprocl " " fmt, ARGloc, ARGprocl(proc_local_ptr))
#define vk_proc_local_dbg(fmt)      DBG(PRloc " " PRprocl " " fmt, ARGloc, ARGprocl(proc_local_ptr))

#endif
