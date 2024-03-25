#ifndef VK_DEBUG_H
#define VK_DEBUG_H

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define ESCAPE_CLEAR "\033[2J"
#define ESCAPE_RESET "\033[;H"

#ifndef DEBUG
#define DEBUG_COND 0
#else
#define DEBUG_COND 1
#endif

#define ERR(...) dprintf(2, __VA_ARGS__)
#define DBG(...) if (DEBUG_COND) ERR(__VA_ARGS__)
#define PERROR(string) ERR("%s: %s\n", string, strerror(errno))

#define vk_tty_clear() ERR("%s", ESCAPE_CLEAR)
#define vk_tty_reset() ERR("%s", ESCAPE_RESET)

#define PRloc "<source" \
    " loc=\"%48s:%4zu\"" \
    ">"
#define ARGloc \
    __FILE__, (size_t) __LINE__

#define vk_klogf(fmt, ...) ERR("      kern: " PRloc ": " fmt,    ARGloc, __VA_ARGS__)
#define vk_kdbgf(fmt, ...) DBG("      kern: " PRloc ": " fmt,    ARGloc, __VA_ARGS__)
#define vk_klog(note)      ERR("      kern: " PRloc ": " "%s\n", ARGloc, note)
#define vk_kdbg(note)      DBG("      kern: " PRloc ": " "%s\n", ARGloc, note)
#define vk_kperror(string) vk_klogf("%s: %s\n", string, strerror(errno))

#endif
