cmake_minimum_required(VERSION 3.27)
project(mnvkd C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_definitions(-D_GNU_SOURCE)

option(DEBUG_MODE "Build the project using the debug mode" ON) # Debug mode is on by default

if(DEBUG_MODE)
    add_definitions(-DDEBUG=1)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-g3 -Os -flto")

add_library(vk vk_kern.c vk_fd_table.c vk_fd.c vk_io_future.c vk_signal.c
        vk_stack.c vk_wrapguard.c vk_heap.c vk_pool.c vk_proc.c vk_proc_local.c
        vk_future.c vk_thread.c vk_socket.c vk_vectoring.c vk_pipe.c
        vk_server.c vk_accepted.c vk_service.c vk_main.c vk_main_local.c)

add_executable(vk_test_echo_service vk_test_echo_service.c vk_echo.c)
target_link_libraries(vk_test_echo_service vk)

add_executable(vk_test_echo_cli vk_test_echo_cli.c vk_echo.c)
target_link_libraries(vk_test_echo_cli vk)

add_executable(vk_test_http11_service vk_test_http11_service.c vk_http11.c vk_fetch.c vk_rfc.c)
target_link_libraries(vk_test_http11_service vk)

add_executable(vk_test_http11_cli vk_test_http11_cli.c vk_http11.c vk_fetch.c vk_rfc.c)
target_link_libraries(vk_test_http11_cli vk)

add_executable(vk_fetch vk_fetch.c vk_rfc.c )
target_link_libraries(vk_fetch vk)

add_executable(vk_test_signal vk_signal.c)
target_link_libraries(vk_test_signal vk)

add_executable(vk_test_cr vk_test_cr.c)
target_link_libraries(vk_test_cr vk)

add_executable(vk_test_log vk_test_log.c)
target_link_libraries(vk_test_log vk)

add_executable(vk_test_exec vk_test_exec.c)
target_link_libraries(vk_test_exec vk)

add_executable(vk_test_mem vk_test_mem.c)
target_link_libraries(vk_test_mem vk)

add_executable(vk_test_ft vk_test_ft.c)
target_link_libraries(vk_test_ft vk)

add_executable(vk_test_ft2 vk_test_ft2.c)
target_link_libraries(vk_test_ft2 vk)

add_executable(vk_test_ft3 vk_test_ft3.c)
target_link_libraries(vk_test_ft3 vk)

add_executable(vk_test_err vk_test_err.c)
target_link_libraries(vk_test_err vk)

add_executable(vk_test_read vk_test_read.c)
target_link_libraries(vk_test_read vk)

add_executable(vk_test_write vk_test_write.c)
target_link_libraries(vk_test_write vk)
