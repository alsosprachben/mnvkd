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
#define PRIloc "%s:%i"
#define ARGloc __FILE__, __LINE__
#define PRIvk "%s()[%s:%i]"
#include "vk_thread.h"
#define ARGvk(that) vk_get_func_name(that), vk_get_file(that), vk_get_line(that)
#define PRIproc "{%zu}"
#define ARGproc(proc_ptr) vk_proc_local_get_proc_id(proc_ptr)
#define vk_log(fmt, ...) ERR(PRIloc " " PRIproc " " PRIvk " " fmt, ARGloc, ARGproc(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__) 
#define vk_dbg(fmt, ...) DBG(PRIloc " " PRIproc " " PRIvk " " fmt, ARGloc, ARGproc(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__) 
#define vk_perror(string) vk_log("%s: %s\n", string, strerror(errno))

#endif
