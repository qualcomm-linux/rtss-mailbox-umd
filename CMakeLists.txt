# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss-mailbox-umd)

cmake_minimum_required(VERSION 3.10)

# Root of the source tree
set(RTSS_ROOT ${CMAKE_CURRENT_LIST_DIR})

# Public API header — version encoded in library SONAME and package name.
set(RTSS_API_INC_PATH      ${RTSS_ROOT}/api)

# Common shared headers (logging etc.)
set(RTSS_COMMON_INC_PATH   ${RTSS_ROOT}/include)

# Safemlib headers — source tree path for building safemlib and mailbox.
set(RTSS_SAFEM_INC_PATH    ${RTSS_ROOT}/middleware/safemlib/include)

# UAPI headers — consumed from KMD sysroot install, not a local copy.
# The KMD exports rtss_mailbox_uapi.h via 'make headers_install'.
# SYSROOTINC_PATH is injected by the Yocto cmake class (STAGING_INCDIR).
if(NOT SYSROOTINC_PATH)
    set(SYSROOTINC_PATH "")
endif()
set(RTSS_UAPI_INC_PATH     ${SYSROOTINC_PATH})

# Middleware library version (safemlib + mailbox).
# Tied to the KMD IOCTL contract — bump SOVERSION when rtss_mailbox_uapi.h
# structs or IOCTL numbers change (i.e. RTSS_MB_UAPI_VERSION_MAJOR bumps).
set(RTSS_MB_LIB_VERSION        "1.0.0")
set(RTSS_MB_LIB_SOVERSION      "1")

# OTA library version — independent of the mailbox IOCTL contract.
# Bump when rtss_ota_lib.h API changes or the OTA wire protocol changes.
# SONAME (librtss_ota.so.1) and package name (qcom-rtss-ota) carry the version.
set(RTSS_OTA_LIB_VERSION       "1.0.0")   # used by apps/rtss-ota/CMakeLists.txt
set(RTSS_OTA_LIB_SOVERSION     "1")

# Output directory for shared libraries: <build_dir>/lib/
set(RTSS_LIB_OUTPUT_DIR    ${CMAKE_CURRENT_BINARY_DIR}/lib)
file(MAKE_DIRECTORY ${RTSS_LIB_OUTPUT_DIR})

# Logging options
#
# RTSS_DEBUG_PRINT: log sink
#   ON  (default) — printf to stdout (console)
#   OFF           — syslog; view with: journalctl -t rtss-mailbox
#   Usage: cmake -DRTSS_DEBUG_PRINT=OFF ..
#
# RTSS_LOG_LEVEL: verbosity (0=none 1=err 2=warn 3=info 4=dbg, default=4)
#   Usage: cmake -DRTSS_LOG_LEVEL=1 ..
option(RTSS_DEBUG_PRINT "Print logs to stdout instead of syslog" ON)
set(RTSS_LOG_LEVEL "4" CACHE STRING "Log verbosity: 0=none 1=err 2=warn 3=info 4=dbg")

if(RTSS_DEBUG_PRINT)
    add_compile_definitions(RTSS_MB_DEBUG_PRINT=1)
else()
    add_compile_definitions(RTSS_MB_DEBUG_PRINT=0)
endif()

add_compile_definitions(RTSS_MB_LOG_LEVEL=${RTSS_LOG_LEVEL})

add_subdirectory(middleware/safemlib   ${CMAKE_CURRENT_BINARY_DIR}/middleware/safemlib)
add_subdirectory(middleware/mailbox    ${CMAKE_CURRENT_BINARY_DIR}/middleware/mailbox)
add_subdirectory(apps/rtss-ota        ${CMAKE_CURRENT_BINARY_DIR}/apps/rtss-ota)
add_subdirectory(apps/rtss-dbg        ${CMAKE_CURRENT_BINARY_DIR}/apps/rtss-dbg)
add_subdirectory(apps/rtss-console    ${CMAKE_CURRENT_BINARY_DIR}/apps/rtss-console)

install(FILES api/rtss_mailbox_api.h DESTINATION include)
