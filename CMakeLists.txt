cmake_minimum_required(VERSION 3.27)
project(mnvkd C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_definitions(-D_GNU_SOURCE)

option(DEBUG_MODE "Build the project using the debug mode" ON) # Debug mode is on by default

if(DEBUG_MODE)
    add_definitions(-DDEBUG=1)
endif()

add_executable(mnvkd
        vk_accepted.c
        vk_accepted.h
        vk_accepted_s.h
        vk_debug.h
        vk_fd.c
        vk_fd.h
        vk_fd_s.h
        vk_fd_table.c
        vk_fd_table.h
        vk_fd_table_s.h
        vk_fetch.c
        vk_fetch.h
        vk_fetch_s.h
        vk_future.c
        vk_future.h
        vk_future_s.h
        vk_heap.c
        vk_heap.h
        vk_heap_s.h
        vk_http11.c
        vk_http11.h
        vk_io_future.c
        vk_io_future.h
        vk_io_future_s.h
        vk_kern.c
        vk_kern.h
        vk_kern_s.h
        vk_pipe.c
        vk_pipe.h
        vk_pipe_s.h
        vk_pool.c
        vk_pool.h
        vk_pool_s.h
        vk_proc.c
        vk_proc.h
        vk_proc_local.c
        vk_proc_local.h
        vk_proc_local_s.h
        vk_proc_s.h
        vk_queue.h
        vk_rfc.c
        vk_rfc.h
        vk_rfc_s.h
        vk_server.c
        vk_server.h
        vk_server_s.h
        vk_service.c
        vk_service.h
        vk_service_s.h
        vk_signal.c
        vk_signal.h
        vk_signal_s.h
        vk_socket.c
        vk_socket.h
        vk_socket_s.h
        vk_stack.c
        vk_stack.h
        vk_stack_s.h
        vk_thread.c
        vk_thread.h
        vk_thread_cr.h
        vk_thread_err.h
        vk_thread_exec.h
        vk_thread_ft.h
        vk_thread_io.h
        vk_thread_mem.h
        vk_thread_s.h
        vk_vectoring.c
        vk_vectoring.h
        vk_vectoring_s.h
        vk_wrapguard.c
        vk_wrapguard.h)
