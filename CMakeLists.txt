cmake_minimum_required(VERSION 3.22)

## macOS does not populate this variable
if(NOT CMAKE_HOST_SYSTEM_PROCESSOR)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE CMAKE_HOST_SYSTEM_PROCESSOR OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
message(STATUS "invoking top level cmake")
message(STATUS "CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")


if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	set(WIN32 "true")
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg triplet" FORCE)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	set(LINUX "true")
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "Vcpkg triplet" FORCE)
        elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(VCPKG_TARGET_TRIPLET "arm64-linux" CACHE STRING "Vcpkg triplet" FORCE)
        endif()
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin") # macOS
	set(APPLE "true")
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "Vcpkg triplet" FORCE)
        elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "Vcpkg triplet" FORCE)
        endif()
    endif()
endif()

if (NOT DEFINED VCPKG_TARGET_TRIPLET)
    message(FATAL_ERROR "VCPKG_TARGET_TRIPLET is not set")
endif()

message(STATUS "Using VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")

## Begin Define Macros
include("${CMAKE_SOURCE_DIR}/macros.cmake")
## End Define Macros

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Toolchain file")
message(STATUS "TOOLCHAIN file is here: " ${CMAKE_TOOLCHAIN_FILE})

if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(CMAKE_BUILD_PARALLEL_LEVEL "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
else()
    set(CMAKE_BUILD_PARALLEL_LEVEL 12)
endif()

# Enable Link Time Optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if (WIN32)
        add_compile_options("/GL")
        add_link_options("/LTCG")
    elseif (CMAKE_GENERATOR STREQUAL "Xcode")
        set(CMAKE_XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL "O3")
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_LTO "YES")
        set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
    else()
        add_compile_options("-flto" "-O3" "-g3")
        add_link_options("-flto" "-O3" "-g3")
#        add_compile_options( "-O3" "-g3")
#        add_link_options( "-O3" "-g3")
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (WIN32)
        add_compile_options("/Zi")
        add_link_options("/DEBUG")
        # Add /FS to handle .pdb access conflicts in parallel builds
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /FS")
    elseif (CMAKE_GENERATOR STREQUAL "Xcode")
        set(CMAKE_XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL "0")
        set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
    else()
        add_compile_options("-g")
        add_link_options("-g")
    endif()
endif()

# make windows debugging not insanely slow
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_CRTDBG_ALLOC_DISABLE")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_ITERATOR_DEBUG_LEVEL=0")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_SECURE_SCL=0")
    #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /O2")
endif()

# seems to be needed
if (NOT DEFINED BUILD_TYPE)
    set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")
endif()

# search vcpkg before system
if (DEFINED VCPKG_ROOT)
    set(CMAKE_PREFIX_PATH "${VCPKG_ROOT}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}" ${CMAKE_PREFIX_PATH})
    set(CMAKE_LIBRARY_PATH "${VCPKG_ROOT}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib" ${CMAKE_LIBRARY_PATH})
    set(CMAKE_INCLUDE_PATH "${VCPKG_ROOT}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include" ${CMAKE_INCLUDE_PATH})
    set(Boost_USE_STATIC_LIBS ON)
    set(BUILD_SHARED_LIBS OFF)
endif()


project(mnvkd C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

include_directories(.)

add_definitions(-D_GNU_SOURCE)

option(DEBUG_MODE "Build the project using the debug mode" ON) # Debug mode is on by default

if(DEBUG_MODE)
    add_definitions(-DDEBUG=1)
endif()

add_definitions(-DUSE_TLS=1)

set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-g3 -Os -flto")

add_library(vk vk_kern.c vk_fd_table.c vk_fd.c vk_io_future.c vk_signal.c
        vk_stack.c vk_wrapguard.c vk_heap.c vk_pool.c vk_proc.c vk_proc_local.c
        vk_future.c vk_thread.c vk_socket.c vk_vectoring.c vk_pipe.c
        vk_server.c vk_accepted.c vk_service.c vk_main.c vk_main_local.c)

target_link_vcpkg_ssl(vk)

add_executable(vk_test_echo_service vk_test_echo_service.c vk_echo.c)
target_link_libraries(vk_test_echo_service vk)

add_executable(vk_test_echo_cli vk_test_echo_cli.c vk_echo.c)
target_link_libraries(vk_test_echo_cli vk)

add_executable(vk_test_http11_service vk_test_http11_service.c vk_http11.c vk_fetch.c vk_rfc.c)
target_link_libraries(vk_test_http11_service PUBLIC vk)

add_executable(vk_test_http11_cli vk_test_http11_cli.c vk_http11.c vk_fetch.c vk_rfc.c)
target_link_libraries(vk_test_http11_cli vk)

add_executable(vk_fetch_test vk_fetch_test.c vk_fetch.c vk_rfc.c )
target_link_libraries(vk_fetch_test vk)

add_executable(vk_test_signal vk_signal.c)
target_compile_definitions(vk_test_signal PRIVATE VK_SIGNAL_TEST=1)
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

add_executable(vk_test_err2 vk_test_err2.c)
target_link_libraries(vk_test_err2 vk)

add_executable(vk_test_read vk_test_read.c)
target_link_libraries(vk_test_read vk)

add_executable(vk_test_write vk_test_write.c)
target_link_libraries(vk_test_write vk)

add_executable(vk_test_forward vk_test_forward.c)
target_link_libraries(vk_test_forward vk)

add_executable(vk_test_pollread vk_test_pollread.c)
target_link_libraries(vk_test_pollread vk)