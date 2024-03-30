cmake_minimum_required(VERSION 3.27)
project(mnvkd C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_definitions(-D_GNU_SOURCE)

option(DEBUG_MODE "Build the project using the debug mode" ON) # Debug mode is on by default

if(DEBUG_MODE)
    add_definitions(-DDEBUG=1)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-g3 -O3")

# creating library vk from vk_kern.c, vk_fd_table.c etc
add_library(vk vk_kern.c vk_fd_table.c vk_fd.c vk_io_future.c vk_signal.c
        vk_stack.c vk_wrapguard.c vk_heap.c vk_pool.c vk_proc.c vk_proc_local.c
        vk_future.c vk_thread.c vk_socket.c vk_vectoring.c vk_pipe.c vk_server.c
        vk_accepted.c vk_service.c)

# creating vk_test executable
add_executable(vk_test vk_test.c)

# creating vk_http11 executable
add_executable(vk_http11 vk_http11.c vk_rfc.c)

# Linking vk library to vk_test executable
target_link_libraries(vk_test vk)

# Linking vk library to vk_http11 executable
target_link_libraries(vk_http11 vk)
