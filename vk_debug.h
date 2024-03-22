#ifndef VK_DEBUG_H
#define VK_DEBUG_H

#include <stdio.h>
#include <errno.h>

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

#define PRloc "<source loc=\"%16s:%4zu\">"
#define ARGloc __FILE__, (size_t) __LINE__

#define PRvk "<thread stage=\"%32s()[%16s:%4zu]\">"
#include "vk_thread.h"
#define ARGvk(that) vk_get_func_name(that), vk_get_file(that), vk_get_line(that)

#define PRprocl "<proc_local id=\"%4zu\" active=\"%c\" blocked=\"%c\")>"
#include "vk_proc_local.h"
#define ARGprocl(proc_local_ptr) vk_proc_local_get_proc_id(proc_local_ptr), (vk_proc_local_get_run(proc_local_ptr) ? 't' : 'f'), (vk_proc_local_get_blocked(proc_local_ptr) ? 't' : 'f')


#define PRvectoring "<vectoring rx=\"%zu+%zu/%zu\" tx=\"%zu+%zu/%zu\" err=\"%s\" block=\"%c%c\" eof=\"%c\" nodata=\"%c\" effect=\"%c\">"
#include "vk_vectoring.h"
#define ARGvectoring(ring) \
    vk_vectoring_rx_cursor(ring), vk_vectoring_rx_len(ring), vk_vectoring_buf_len(ring), \
    vk_vectoring_tx_cursor(ring), vk_vectoring_tx_len(ring), vk_vectoring_buf_len(ring), \
    strerror(vk_vectoring_has_error(ring)), \
    vk_vectoring_rx_is_blocked(ring) ? 'r' : ' ', \
    vk_vectoring_tx_is_blocked(ring) ? 'w' : ' ', \
    vk_vectoring_has_eof(ring) ? 't' : 'f', \
    vk_vectoring_has_nodata(ring) ? 't' : 'f', \
    vk_vectoring_has_effect(ring) ? 't' : 'f'
#define PRvectoring_tx "<vectoring_tx buf=\"%.*s%.*s\">"
#define ARGvectoring_tx(ring) \
    vk_vectoring_tx_buf1_len(ring), vk_vectoring_tx_buf1(ring), \
    vk_vectoring_tx_buf2_len(ring), vk_vectoring_tx_buf2(ring)
#define PRvectoring_rx "<vectoring_rx buf=\"%.*s%.*s\">"
#define ARGvectoring_rx(ring) \
    vk_vectoring_rx_buf1_len(ring), vk_vectoring_rx_buf1(ring), \
    vk_vectoring_rx_buf2_len(ring), vk_vectoring_rx_buf2(ring)

#define PRsocket "<socket rx_fd=\"%4i\" tx_rd=\"%4i\" error=\"%4i\" blocked=\"%c\" blocked_op=\"%s\">"
#include "vk_socket.h"
#define ARGsocket(socket_ptr) \
    vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), \
    vk_pipe_get_fd(vk_socket_get_tx_fd(socket_ptr)), \
    vk_socket_get_error(socket_ptr),                 \
    vk_socket_get_enqueued_blocked(socket_ptr) ? 't' : 'f', \
    ((vk_block_get_op(vk_socket_get_block(socket_ptr)) != 0) ? vk_block_get_op_str(vk_socket_get_block(socket_ptr)) : "")

#define PRproc "<proc       id=\"%4zu\" active=\"%c\" blocked=\"%c\")>"
#include "vk_proc.h"
#define ARGproc(proc_ptr) vk_proc_get_id(proc_ptr), (vk_proc_get_run(proc_ptr) ? 't' : 'f'), (vk_proc_get_blocked(proc_ptr) ? 't' : 'f')

#define PRfd "<fd fd=\"%i\" proc_id=\"%zu\">"
#include "vk_fd.h"
#define ARGfd(fd_ptr) vk_fd_get_fd(fd_ptr), vk_fd_get_proc_id(fd_ptr)

#define vk_klogf(fmt, ...) ERR("      kern: " PRloc ": " fmt,    ARGloc, __VA_ARGS__)
#define vk_kdbgf(fmt, ...) DBG("      kern: " PRloc ": " fmt,    ARGloc, __VA_ARGS__)
#define vk_klog(note)      ERR("      kern: " PRloc ": " "%s\n", ARGloc, note)
#define vk_kdbg(note)      DBG("      kern: " PRloc ": " "%s\n", ARGloc, note)
#define vk_kperror(string) vk_klogf("%s: %s\n", string, strerror(errno))

#define vk_logf(fmt, ...) ERR("    thread: " PRloc " " PRprocl " " PRvk ": " fmt,    ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__)
#define vk_dbgf(fmt, ...) DBG("    thread: " PRloc " " PRprocl " " PRvk ": " fmt,    ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__)
#define vk_log(note)      ERR("    thread: " PRloc " " PRprocl " " PRvk ": " "%s\n", ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), note)
#define vk_dbg(note)      DBG("    thread: " PRloc " " PRprocl " " PRvk ": " "%s\n", ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), note)
#define vk_perror(string) vk_logf("%s: %s\n", string, strerror(errno))

#define vk_proc_local_logf(fmt, ...) ERR("proc_local: " PRloc " " PRprocl ": " fmt,    ARGloc, ARGprocl(proc_local_ptr), __VA_ARGS__)
#define vk_proc_local_dbgf(fmt, ...) DBG("proc_local: " PRloc " " PRprocl ": " fmt,    ARGloc, ARGprocl(proc_local_ptr), __VA_ARGS__)
#define vk_proc_local_log(note)      ERR("proc_local: " PRloc " " PRprocl ": " "%s\n", ARGloc, ARGprocl(proc_local_ptr), note)
#define vk_proc_local_dbg(note)      DBG("proc_local: " PRloc " " PRprocl ": " "%s\n", ARGloc, ARGprocl(proc_local_ptr), note)
#define vk_proc_local_perror(string) vk_proc_local_logf("%s: %s\n", string, strerror(errno))

#define vk_vectoring_logf(fmt, ...) ERR(" vectoring: " PRloc " " PRvectoring ": " fmt,    ARGloc, ARGvectoring(ring), __VA_ARGS__)
#define vk_vectoring_dbgf(fmt, ...) DBG(" vectoring: " PRloc " " PRvectoring ": " fmt,    ARGloc, ARGvectoring(ring), __VA_ARGS__)
#define vk_vectoring_log(note)      ERR(" vectoring: " PRloc " " PRvectoring ": " "%s\n", ARGloc, ARGvectoring(ring), note)
#define vk_vectoring_dbg(note)      DBG(" vectoring: " PRloc " " PRvectoring ": " "%s\n", ARGloc, ARGvectoring(ring), note)
#define vk_vectoring_perror(string) vk_vectoring_logf("%s: %s\n", string, strerror(errno))
#define vk_vectoring_logf_tx(fmt, ...) ERR(" vectoring: " PRloc " " PRvectoring " " PRvectoring_tx ": " fmt,    ARGloc, ARGvectoring(ring), ARGvectoring_tx(ring), __VA_ARGS__)
#define vk_vectoring_dbgf_tx(fmt, ...) DBG(" vectoring: " PRloc " " PRvectoring " " PRvectoring_tx ": " fmt,    ARGloc, ARGvectoring(ring), ARGvectoring_tx(ring), __VA_ARGS__)
#define vk_vectoring_logf_rx(fmt, ...) ERR(" vectoring: " PRloc " " PRvectoring " " PRvectoring_rx ": " fmt,    ARGloc, ARGvectoring(ring), ARGvectoring_rx(ring), __VA_ARGS__)
#define vk_vectoring_dbgf_rx(fmt, ...) DBG(" vectoring: " PRloc " " PRvectoring " " PRvectoring_rx ": " fmt,    ARGloc, ARGvectoring(ring), ARGvectoring_rx(ring), __VA_ARGS__)

#define vk_socket_logf(fmt, ...) ERR("    socket: " PRloc " " PRsocket ": " fmt, ARGloc,    ARGsocket(socket_ptr), __VA_ARGS__)
#define vk_socket_dbgf(fmt, ...) DBG("    socket: " PRloc " " PRsocket ": " fmt, ARGloc,    ARGsocket(socket_ptr), __VA_ARGS__)
#define vk_socket_log(note)      ERR("    socket: " PRloc " " PRsocket ": " "%s\n", ARGloc, ARGsocket(socket_ptr), note)
#define vk_socket_dbg(note)      DBG("    socket: " PRloc " " PRsocket ": " "%s\n", ARGloc, ARGsocket(socket_ptr), note)
#define vk_socket_perror(string) vk_socket_logf("%s: %s\n", string, strerror(vk_socket_get_error(socket_ptr)))

#define vk_proc_logf(fmt, ...) ERR("      proc: " PRloc " " PRproc ": " fmt,    ARGloc, ARGproc(proc_ptr), __VA_ARGS__)
#define vk_proc_dbgf(fmt, ...) DBG("      proc: " PRloc " " PRproc ": " fmt,    ARGloc, ARGproc(proc_ptr), __VA_ARGS__)
#define vk_proc_log(note)      ERR("      proc: " PRloc " " PRproc ": " "%s\n", ARGloc, ARGproc(proc_ptr), note)
#define vk_proc_dbg(note)      DBG("      proc: " PRloc " " PRproc ": " "%s\n", ARGloc, ARGproc(proc_ptr), note)
#define vk_proc_perror(string) vk_proc_logf("%s: %s\n", string, strerror(errno))

#define vk_fd_logf(fmt, ...) ERR(PRloc " " PRfd ": " fmt,    ARGloc, ARGfd(fd_ptr), __VA_ARGS__)
#define vk_fd_dbgf(fmt, ...) DBG(PRloc " " PRfd ": " fmt,    ARGloc, ARGfd(fd_ptr), __VA_ARGS__)
#define vk_fd_log(note)      ERR(PRloc " " PRfd ": " "%s\n", ARGloc, ARGfd(fd_ptr), note)
#define vk_fd_dbg(note)      DBG(PRloc " " PRfd ": " "%s\n", ARGloc, ARGfd(fd_ptr), note)
#define vk_fd_perror(string) vk_fd_logf("%s: %s\n", string, strerror(errno))

#endif
