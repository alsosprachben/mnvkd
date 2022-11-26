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

#define PRloc "%16s:%4i"
#define ARGloc __FILE__, __LINE__

#define PRvk "thread %32s()[%16s:%4i]:"
#include "vk_thread.h"
#define ARGvk(that) vk_get_func_name(that), vk_get_file(that), vk_get_line(that)

#define PRprocl " local process %4zu:"
#include "vk_proc_local.h"
#define ARGprocl(proc_local_ptr) vk_proc_local_get_proc_id(proc_local_ptr)

#define PRsocket "socket [rx: %i, tx: %i, block[%i: %s]]:"
#include "vk_socket.h"
#define ARGsocket(socket_ptr) vk_pipe_get_fd(vk_socket_get_rx_fd(socket_ptr)), vk_pipe_get_fd(vk_socket_get_tx_fd(socket_ptr)), vk_block_get_fd(vk_socket_get_block(socket_ptr)), ((vk_block_get_op(vk_socket_get_block(socket_ptr)) != 0) ? vk_block_get_op_str(vk_socket_get_block(socket_ptr)) : "")

#define PRproc "global process %4zu:"
#include "vk_proc.h"
#define ARGproc(proc_ptr) vk_proc_get_id(proc_ptr)

#define vk_logf(fmt, ...) ERR(PRloc " " PRprocl " " PRvk " " fmt,    ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__)
#define vk_dbgf(fmt, ...) DBG(PRloc " " PRprocl " " PRvk " " fmt,    ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), __VA_ARGS__)
#define vk_log(note)      ERR(PRloc " " PRprocl " " PRvk " " "%s\n", ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), note)
#define vk_dbg(note)      DBG(PRloc " " PRprocl " " PRvk " " "%s\n", ARGloc, ARGprocl(vk_get_proc_local(that)), ARGvk(that), note)
#define vk_perror(string) vk_logf("%s: %s\n", string, strerror(errno))

#define vk_proc_local_logf(fmt, ...) ERR(PRloc " " PRprocl " " fmt,    ARGloc, ARGprocl(proc_local_ptr), __VA_ARGS__)
#define vk_proc_local_dbgf(fmt, ...) DBG(PRloc " " PRprocl " " fmt,    ARGloc, ARGprocl(proc_local_ptr), __VA_ARGS__)
#define vk_proc_local_log(note)      ERR(PRloc " " PRprocl " " "%s\n", ARGloc, ARGprocl(proc_local_ptr), note)
#define vk_proc_local_dbg(note)      DBG(PRloc " " PRprocl " " "%s\n", ARGloc, ARGprocl(proc_local_ptr), note)

#define vk_socket_logf(fmt, ...) ERR(PRloc " " PRsocket " " fmt, ARGloc,    ARGsocket(socket_ptr), __VA_ARGS__)
#define vk_socket_dbgf(fmt, ...) DBG(PRloc " " PRsocket " " fmt, ARGloc,    ARGsocket(socket_ptr), __VA_ARGS__)
#define vk_socket_log(note)      ERR(PRloc " " PRsocket " " "%s\n", ARGloc, ARGsocket(socket_ptr), note)
#define vk_socket_dbg(note)      DBG(PRloc " " PRsocket " " "%s\n", ARGloc, ARGsocket(socket_ptr), note)

#define vk_proc_logf(fmt, ...) ERR(PRloc " " PRproc " " fmt,    ARGloc, ARGproc(proc_ptr), __VA_ARGS__)
#define vk_proc_dbgf(fmt, ...) DBG(PRloc " " PRproc " " fmt,    ARGloc, ARGproc(proc_ptr), __VA_ARGS__)
#define vk_proc_log(note)      ERR(PRloc " " PRproc " " "%s\n", ARGloc, ARGproc(proc_ptr), note)
#define vk_proc_dbg(note)      DBG(PRloc " " PRproc " " "%s\n", ARGloc, ARGproc(proc_ptr), note)

#endif