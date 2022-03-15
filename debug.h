#ifndef DEBUG_H
#define DEBUG_H

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
#define ARGvk(that) that->func_name, that->file, that->line
#define vk_log(fmt, ...) ERR(PRIloc " " PRIvk " " fmt, ARGloc, ARGvk(that), __VA_ARGS__) 
#define vk_perror(string) vk_log("%s: %s\n", string, strerror(errno))

#endif
