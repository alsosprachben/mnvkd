include("../vcpkg/triplets/community/arm64-linux.cmake")

set(VCPKG_TARGET_ARCHITECTURE arm64)   # Set architecture to arm64 for ARM Linux
set(VCPKG_CRT_LINKAGE dynamic)         # Link the C runtime dynamically
set(VCPKG_LIBRARY_LINKAGE static)      # Link all other libraries statically

if (VCPKG_BUILD_TYPE STREQUAL "release")
    # Enable Link-Time Optimization (LTO)
    set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} -flto")
    set(VCPKG_C_FLAGS "${VCPKG_C_FLAGS} -flto")
    set(VCPKG_LINKER_FLAGS "${VCPKG_LINKER_FLAGS} -flto")
    set(VCPKG_EXE_LINKER_FLAGS "${VCPKG_EXE_LINKER_FLAGS} -flto")
endif()
