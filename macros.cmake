# platform determination

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(WIN32 "true")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX "true")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin") # macOS
    set(APPLE "true")
endif()

function(startswith VAR PREFIX RESULT_VAR)
    string(FIND "${VAR}" "${PREFIX}" POSITION)
    message(STATUS "Found `${PREFIX}` in `${VAR}` at position ${POSITION}") 
    if (POSITION EQUAL 0)
        message(STATUS "TRUE")
        set(${RESULT_VAR} "TRUE" PARENT_SCOPE)
    else()
        message(STATUS "FALSE")
        set(${RESULT_VAR} "FALSE" PARENT_SCOPE)
    endif()
    message(STATUS "RESULT_VAR ${RESULT_VAR}")
endfunction()

# Enqueue subdirectory list; this is intended to be used with `add_enqueued_subdirectories()`
macro(enqueue_subdirectories)
    foreach (subDir ${ARGN})
        list(APPEND sub_directories ${subDir})
    endforeach ()
endmacro()

# Add (distinct) subdirectories contained within enqueued subdirectory list; which where previously included with `enqueue_subdirectories(...)`
macro(add_enqueued_subdirectories)
    list(REMOVE_DUPLICATES sub_directories)
    foreach (subDir ${sub_directories})
        MESSAGE("Adding subdirectory: ${CMAKE_CURRENT_LIST_DIR}/${subDir}")
        add_subdirectory(${subDir})
    endforeach ()
endmacro()

macro(target_include_vcpkg TARGET_NAME)
    set(VCPKG_INSTALLED_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
    target_include_directories(${TARGET_NAME} PUBLIC "${VCPKG_INSTALLED_PATH}/include")
endmacro()

# Helper: Generate a list of glob patterns for a given LIB_DIR, library base name, and configuration.
function(get_library_patterns LIB_DIR LIB_BASE CONFIG_TYPE OUTPUT_VAR)
  if(WIN32)
    if(CONFIG_TYPE STREQUAL "debug")
      # For Windows debug, explicitly enumerate:
      #   - One variant with a hyphen before the trailing "d" (e.g. "boost_chrono-vc143-mt-x64-1_86-d.lib")
      #   - One variant with no hyphen (e.g. "boost_chronod.lib")
      #   - Both with and without the "lib" prefix.
      set(PATTERNS
        "${LIB_DIR}/${LIB_BASE}-d.lib"
        "${LIB_DIR}/${LIB_BASE}d.lib"
        "${LIB_DIR}/lib${LIB_BASE}-d.lib"
        "${LIB_DIR}/lib${LIB_BASE}d.lib"
        "${LIB_DIR}/${LIB_BASE}lib-d.lib"
        "${LIB_DIR}/${LIB_BASE}libd.lib"
        # Fallback to standard name if no debug-specific suffix is applied.
        "${LIB_DIR}/${LIB_BASE}.lib"
        "${LIB_DIR}/lib${LIB_BASE}.lib"
        "${LIB_DIR}/${LIB_BASE}lib.lib"
      )
    else()  # Release configuration
      # Expect the exact release name, with or without a "lib" prefix.
      set(PATTERNS
        "${LIB_DIR}/${LIB_BASE}.lib"
        "${LIB_DIR}/lib${LIB_BASE}.lib"
        "${LIB_DIR}/${LIB_BASE}lib.lib"
      )
    endif()
  elseif(APPLE)
    if(CONFIG_TYPE STREQUAL "debug")
      # For macOS debug, enumerate common suffix variants:
      set(PATTERNS
        "${LIB_DIR}/lib${LIB_BASE}-debug.a"     # e.g. libyaml-cpp-debug.a
        "${LIB_DIR}/lib${LIB_BASE}-debug.dylib"
        "${LIB_DIR}/lib${LIB_BASE}_debug.a"       # e.g. libhdf5_debug.a
        "${LIB_DIR}/lib${LIB_BASE}_debug.dylib"
        "${LIB_DIR}/lib${LIB_BASE}-d.a"           # e.g. libcurl-d.a
        "${LIB_DIR}/lib${LIB_BASE}-d.dylib"
        "${LIB_DIR}/lib${LIB_BASE}d.a"            # e.g. libyaml-cppd.a
        "${LIB_DIR}/lib${LIB_BASE}d.dylib"
        # Fallback to standard name if no debug-specific suffix is applied.
        "${LIB_DIR}/lib${LIB_BASE}.a"
        "${LIB_DIR}/lib${LIB_BASE}.dylib"
      )
    else()
      # macOS release: standard naming.
      set(PATTERNS
        "${LIB_DIR}/lib${LIB_BASE}.a"
        "${LIB_DIR}/lib${LIB_BASE}.dylib"
      )
    endif()
  elseif(LINUX)
    if(CONFIG_TYPE STREQUAL "debug")
      # Linux debug: enumerate common debug suffix variants.
      set(PATTERNS
        "${LIB_DIR}/lib${LIB_BASE}-debug.a"    # e.g. libhdf5-debug.a
        "${LIB_DIR}/lib${LIB_BASE}_debug.a"      # e.g. libhdf5_debug.a
        "${LIB_DIR}/lib${LIB_BASE}-d.a"          # e.g. libcurl-d.a
        "${LIB_DIR}/lib${LIB_BASE}d.a"           # e.g. libyaml-cppd.a
        # Fallback to standard name.
        "${LIB_DIR}/lib${LIB_BASE}.a"
        "${LIB_DIR}/lib${LIB_BASE}.so"
      )
    else()
      # Linux release: standard naming.
      set(PATTERNS
        "${LIB_DIR}/lib${LIB_BASE}.a"
        "${LIB_DIR}/lib${LIB_BASE}.so"
      )
    endif()
  else()
    set(PATTERNS "${LIB_DIR}/${LIB_BASE}.lib")
  endif()
  set(${OUTPUT_VAR} "${PATTERNS}" PARENT_SCOPE)
endfunction()

# Main function: Find a vcpkg library given its base name.
function(find_vcpkg_library RESULT_VAR LIB_BASE)
  message(STATUS "find_vcpkg_library(${LIB_BASE})")
  set(VCPKG_INSTALLED_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
  include_directories("${VCPKG_INSTALLED_PATH}/include")

  if(CMAKE_CONFIGURATION_TYPES)  # Multi-config generators (e.g. Visual Studio)
    set(DEBUG_LIB_DIR "${VCPKG_INSTALLED_PATH}/debug/lib")
    set(RELEASE_LIB_DIR "${VCPKG_INSTALLED_PATH}/lib")

    get_library_patterns(${DEBUG_LIB_DIR} ${LIB_BASE} "debug" DEBUG_PATTERNS)
    file(GLOB FOUND_DEBUG ${DEBUG_PATTERNS})
    if(NOT FOUND_DEBUG)
      message(FATAL_ERROR "Could not find debug version of ${LIB_BASE} in ${DEBUG_LIB_DIR}")
    else()
      list(GET FOUND_DEBUG 0 DEBUG_LIB)
    endif()

    get_library_patterns(${RELEASE_LIB_DIR} ${LIB_BASE} "release" RELEASE_PATTERNS)
    file(GLOB FOUND_RELEASE ${RELEASE_PATTERNS})
    if(NOT FOUND_RELEASE)
      message(FATAL_ERROR "Could not find release version of ${LIB_BASE} in ${RELEASE_LIB_DIR}")
    else()
      list(GET FOUND_RELEASE 0 RELEASE_LIB)
    endif()

    message(STATUS "Found debug library: ${DEBUG_LIB}")
    message(STATUS "Found release library: ${RELEASE_LIB}")
    # Return a semicolon-separated list for multi-config generators:
    #   target_link_libraries(... debug <debug_lib> optimized <release_lib> ...)
    set(${RESULT_VAR} "debug;${DEBUG_LIB};optimized;${RELEASE_LIB}" PARENT_SCOPE)
  else()  # Single-config generators (e.g. Ninja, Makefiles)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(LIB_DIR "${VCPKG_INSTALLED_PATH}/debug/lib")
      set(CONFIG_TYPE "debug")
    else()
      set(LIB_DIR "${VCPKG_INSTALLED_PATH}/lib")
      set(CONFIG_TYPE "release")
    endif()

    get_library_patterns(${LIB_DIR} ${LIB_BASE} ${CONFIG_TYPE} PATTERNS)
    file(GLOB FOUND_LIBRARIES ${PATTERNS})
    if(NOT FOUND_LIBRARIES)
      message(FATAL_ERROR "Could not find ${LIB_BASE} in ${LIB_DIR}")
    else()
      list(GET FOUND_LIBRARIES 0 FOUND_LIBRARY)
    endif()
    message(STATUS "Found library: ${FOUND_LIBRARY}")
    set(${RESULT_VAR} "${FOUND_LIBRARY}" PARENT_SCOPE)
  endif()
endfunction()

macro(log_search TARGET_NAME LIB)
	message(STATUS "Searching for ${LIB} for ${TARGET_NAME}")
endmacro()

macro(target_link_standard TARGET_NAME)
    log_search(${TARGET_NAME} "standard libraries")
    if (NOT WIN32)
        target_link_libraries(${TARGET_NAME} PUBLIC pthread)
        target_link_libraries(${TARGET_NAME} PUBLIC stdc++)
    endif()
    if (LINUX)
	target_link_libraries(${TARGET_NAME} PUBLIC dl)
        target_link_libraries(${TARGET_NAME} PUBLIC m)
        target_link_libraries(${TARGET_NAME} PUBLIC stdc++fs)
        target_link_libraries(${TARGET_NAME} PUBLIC stdc++)
    endif()
	target_include_vcpkg(${TARGET_NAME})
endmacro()

macro(target_link_vcpkg_boost TARGET_NAME)
    log_search(${TARGET_NAME} "boost")
    # Boost Chrono (no dependencies, can be linked early)
    find_vcpkg_library(BOOST_CHRONO_LIBRARY "boost_chrono*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${BOOST_CHRONO_LIBRARY})

    # Boost Log Setup (depends on Boost Log)
    find_vcpkg_library(BOOST_LOG_SETUP_LIBRARY "boost_log_setup*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${BOOST_LOG_SETUP_LIBRARY})
 
    # Boost Log (depends on other Boost libraries)
    find_vcpkg_library(BOOST_LOG_LIBRARY "boost_log*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${BOOST_LOG_LIBRARY})
 
    # Boost Filesystem (depends on Boost System)
    find_vcpkg_library(BOOST_FILESYSTEM_LIBRARY "boost_filesystem*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${BOOST_FILESYSTEM_LIBRARY})

    # Boost System (required for Boost Log and other components)
    find_vcpkg_library(BOOST_SYSTEM_LIBRARY "boost_system*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${BOOST_SYSTEM_LIBRARY})

    # Boost Thread (no complex dependencies)
    find_vcpkg_library(BOOST_THREAD_LIBRARY "boost_thread*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${BOOST_THREAD_LIBRARY})

    # Boost Process (depends on Boost Asio and other components)
    find_vcpkg_library(BOOST_PROCESS_LIBRARY "boost_process*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${BOOST_PROCESS_LIBRARY})
    
    target_link_standard(${TARGET_NAME})
	target_include_vcpkg(${TARGET_NAME})
endmacro()

macro(target_link_vcpkg_ssl TARGET_NAME)
    log_search(${TARGET_NAME} "ssl")

    # OpenSSL SSL (required for SSL connections)
    find_vcpkg_library(OPENSSL_SSL_LIBRARY "ssl*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${OPENSSL_SSL_LIBRARY})

    # OpenSSL Crypto (required for encryption)
    find_vcpkg_library(OPENSSL_CRYPTO_LIBRARY "crypto*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
	target_include_vcpkg(${TARGET_NAME})

    # mac version of crypto support functions
    if(APPLE)
        target_link_libraries(${TARGET_NAME} PUBLIC
            "-framework CoreFoundation"
            "-framework Security"
            "-framework SystemConfiguration"
        )
    endif()
    if(LINUX)
	    target_link_libraries(${TARGET_NAME} PUBLIC
		    pthread
		    dl
		    )
    endif(LINUX)
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PUBLIC
            wininet    # For WinINet functions
            ncrypt     # For NCrypt functions
            shlwapi    # For Shell Utility functions
            Crypt32    # You may already have this for certificate-related functions
            Winhttp    # For WinHTTP functions
            Ws2_32     # For Winsock functions
            Userenv    # For User Profile functions
            Version    # For version functions (e.g., GetFileVersionInfo)
            WSock32    # For Winsock functions
        )
    endif()

endmacro()
    

macro(target_link_vcpkg_curl TARGET_NAME)
    log_search(${TARGET_NAME} "curl")
    # CURL (required for HTTP operations)
    find_vcpkg_library(CURL_LIBRARY "curl*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${CURL_LIBRARY})

    # Zlib
    find_vcpkg_library(ZLIB_LIBRARY "z")
    target_link_libraries(${TARGET_NAME} PUBLIC ${ZLIB_LIBRARY})

    # OpenSSL SSL (required for SSL connections)
    find_vcpkg_library(OPENSSL_SSL_LIBRARY "ssl*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${OPENSSL_SSL_LIBRARY})

    # OpenSSL Crypto (required for encryption)
    find_vcpkg_library(OPENSSL_CRYPTO_LIBRARY "crypto*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
	target_include_vcpkg(${TARGET_NAME})

    # mac version of crypto support functions
    if(APPLE)
        target_link_libraries(${TARGET_NAME} PUBLIC
            "-framework CoreFoundation"
            "-framework Security"
            "-framework SystemConfiguration"
        )
    endif()
    if(LINUX)
	    target_link_libraries(${TARGET_NAME} PUBLIC
		    pthread
		    dl
		    )
    endif(LINUX)
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PUBLIC
            wininet    # For WinINet functions
            ncrypt     # For NCrypt functions
            shlwapi    # For Shell Utility functions
            Crypt32    # You may already have this for certificate-related functions
            Winhttp    # For WinHTTP functions
            Ws2_32     # For Winsock functions
            Userenv    # For User Profile functions
            Version    # For version functions (e.g., GetFileVersionInfo)
            WSock32    # For Winsock functions
        )
    endif()

endmacro()

macro(target_link_vcpkg_awssdk TARGET_NAME)
    log_search(${TARGET_NAME} "awssdk")
    # AWS SDK - S3 (depends on Core and other libraries)
    find_vcpkg_library(AWS_SDK_S3_LIBRARY "aws-cpp-sdk-s3*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_SDK_S3_LIBRARY})

    # AWS SDK - Core (depends on lower-level libraries like AWS CRT)
    find_vcpkg_library(AWS_SDK_CORE_LIBRARY "aws-cpp-sdk-core*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_SDK_CORE_LIBRARY})

    # AWS Crt (includes necessary components like Crypto, HTTP, and Endpoints)
    find_vcpkg_library(AWS_CRT_LIBRARY "aws-crt-cpp*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_CRT_LIBRARY})

    # AWS C Event Stream (depends on IO, Common)
    find_vcpkg_library(AWS_C_EVENT_STREAM_LIBRARY "aws-c-event-stream*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_EVENT_STREAM_LIBRARY})

    # AWS C MQTT (depends on IO, Common)
    find_vcpkg_library(AWS_C_MQTT_LIBRARY "aws-c-mqtt*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_MQTT_LIBRARY})

    # AWS C S3 (depends on HTTP, IO, Common, Checksums)
    find_vcpkg_library(AWS_C_S3_LIBRARY "aws-c-s3*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_S3_LIBRARY})

    # AWS C Auth (depends on Common, Event Stream, SSL, Crypto, HTTP)
    find_vcpkg_library(AWS_C_AUTH_LIBRARY "aws-c-auth*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_AUTH_LIBRARY})

    # AWS C HTTP (depends on IO, Common, and Curl)
    find_vcpkg_library(AWS_C_HTTP_LIBRARY "aws-c-http*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_HTTP_LIBRARY})

    # AWS C IO (depends on Common)
    find_vcpkg_library(AWS_C_IO_LIBRARY "aws-c-io*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_IO_LIBRARY})

    # AWS C Compression (for Huffman and compression functions)
    find_vcpkg_library(AWS_C_COMPRESSION_LIBRARY "aws-c-compression*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_COMPRESSION_LIBRARY})

    if (LINUX)
        # S2N TLS (for TLS functions)
        find_vcpkg_library(S2N_LIBRARY "s2n*")
        target_link_libraries(${TARGET_NAME} PUBLIC ${S2N_LIBRARY})
    endif()

    # AWS Checksums (used for integrity checking)
    find_vcpkg_library(AWS_CHECKSUMS_LIBRARY "aws-checksums*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_CHECKSUMS_LIBRARY})

    # AWS Checksums (used for integrity checking)
    find_vcpkg_library(AWS_C_CAL_LIBRARY "aws-c-cal*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_CAL_LIBRARY})

    # AWS SDK Utils
    find_vcpkg_library(AWS_SDK_UTILS_LIBRARY "aws-c-sdkutils*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_SDK_UTILS_LIBRARY})

    # AWS C Common (fundamental library for other AWS C libraries)
    find_vcpkg_library(AWS_C_COMMON_LIBRARY "aws-c-common*")
    target_link_libraries(${TARGET_NAME} PUBLIC ${AWS_C_COMMON_LIBRARY})

    target_link_vcpkg_curl(${TARGET_NAME})

    if (WIN32)
    target_link_libraries(${TARGET_NAME} PUBLIC Secur32)
    endif()

	if(LINUX)
		find_library(DL_LIB dl REQUIRED)
		if(DL_LIB)
			target_link_libraries(${TARGET_NAME} PUBLIC ${DL_LIB})
		endif()
	endif()
	target_include_vcpkg(${TARGET_NAME})
endmacro()

macro(target_link_vcpkg_netcdf TARGET_NAME)
    log_search(${TARGET_NAME} "netcdf")
	find_vcpkg_library(NETCDF_LIBRARY "netcdf")
	target_link_libraries(${TARGET_NAME} PUBLIC ${NETCDF_LIBRARY})
	find_vcpkg_library(HDF5HL_LIBRARY "hdf5_hl")
	target_link_libraries(${TARGET_NAME} PUBLIC ${HDF5HL_LIBRARY})
	find_vcpkg_library(HDF5_LIBRARY "hdf5")
	target_link_libraries(${TARGET_NAME} PUBLIC ${HDF5_LIBRARY})
	target_include_vcpkg(${TARGET_NAME})
endmacro()

macro(target_link_vcpkg_fftw3 TARGET_NAME)
    log_search(${TARGET_NAME} "fftw3")
	find_vcpkg_library(FFTW3_LIBRARY "fftw3")
	target_link_libraries(${TARGET_NAME} PUBLIC ${FFTW3_LIBRARY})
	find_vcpkg_library(FFTW3F_LIBRARY "fftw3f")
	target_link_libraries(${TARGET_NAME} PUBLIC ${FFTW3F_LIBRARY})
	find_vcpkg_library(FFTW3L_LIBRARY "fftw3l")
	target_link_libraries(${TARGET_NAME} PUBLIC ${FFTW3L_LIBRARY})
	target_include_vcpkg(${TARGET_NAME})
endmacro()
	
macro(target_link_vcpkg_png TARGET_NAME)
    log_search(${TARGET_NAME} "libpng")
	find_vcpkg_library(PNG_LIBRARY "png")
	target_link_libraries(${TARGET_NAME} PUBLIC ${PNG_LIBRARY})
	target_include_vcpkg(${TARGET_NAME})
endmacro()

macro(target_link_vcpkg_z TARGET_NAME)
    log_search(${TARGET_NAME} "libz")
    find_vcpkg_library(ZLIB_LIBRARY "z")
    target_link_libraries(${TARGET_NAME} PUBLIC ${ZLIB_LIBRARY})
endmacro()

macro(target_link_vcpkg_glut TARGET_NAME)
    log_search(${TARGET_NAME} "libglut")
    find_vcpkg_library(GLUTLIB_LIBRARY "glut")
    target_link_libraries(${TARGET_NAME} PUBLIC ${GLUTLIB_LIBRARY})
endmacro()

macro(target_link_vcpkg_ncurses TARGET_NAME)
    log_search(${TARGET_NAME} "ncurses")
    find_vcpkg_library(NCURSESLIB_LIBRARY "ncurses")
    target_link_libraries(${TARGET_NAME} PUBLIC ${NCURSESLIB_LIBRARY})
endmacro()
