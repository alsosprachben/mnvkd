#ifndef DEBUG_H
#define DEBUG_H

#ifndef DEBUG
#define DEBUG_COND 0
#else
#define DEBUG_COND 1
#endif

#define ERR(...) dprintf(2, __VA_ARGS__)
#define DBG(...) if (DEBUG_COND) ERR(__VA_ARGS__)
#define PRIvk "%s()[%s:%i]"
#define ARGvk(that) that->func_name, that->file, that->line

#endif
