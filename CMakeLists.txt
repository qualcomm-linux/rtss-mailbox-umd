# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

cmake_minimum_required(VERSION 3.10)

# PROJECT_VERSION is the single release version for this repo, tracking the
# git release tag (vX.Y.Z). Bump this on every tagged release; the Yocto
# recipe's PV must be kept in sync with the same value.
project(rtss-mailbox-umd VERSION 1.0.0)

# Root of the source tree
set(RTSS_ROOT ${CMAKE_CURRENT_LIST_DIR})

# Public API header — version encoded in library SONAME and package name.
set(RTSS_API_INC_PATH      ${RTSS_ROOT}/api)

# Common shared headers (logging etc.)
set(RTSS_COMMON_INC_PATH   ${RTSS_ROOT}/include)

# Safemlib headers — source tree path for building safemlib and mailbox.
set(RTSS_SAFEM_INC_PATH    ${RTSS_ROOT}/middleware/safemlib/include)

# Middleware library version (safemlib + mailbox).
# VERSION tracks PROJECT_VERSION (the release tag) so every .so rev moves in
# lockstep with a tagged release. SOVERSION is the separate ABI-contract number:
# bump it only when rtss_mailbox_uapi.h structs or IOCTL numbers change
# (i.e. RTSS_MB_UAPI_VERSION_MAJOR bumps), not on every release tag.
set(RTSS_MB_LIB_VERSION        "${PROJECT_VERSION}")
set(RTSS_MB_LIB_SOVERSION      "1")

# Updater library version — also tracks PROJECT_VERSION. SOVERSION stays
# independent of the mailbox IOCTL contract: bump it when rtss_updater_lib.h
# API changes or the updater wire protocol changes.
# SONAME (librtss_updater.so.1) and package name (qcom-rtss-updater) carry the version.
set(RTSS_UPDATE_LIB_VERSION       "${PROJECT_VERSION}")   # used by apps/rtss-updater/CMakeLists.txt
set(RTSS_UPDATE_LIB_SOVERSION     "1")

# GPT helper library version — also tracks PROJECT_VERSION. SOVERSION stays
# independent: bump it when rtss_guidpt.h API changes.
set(RTSS_GUIDPT_LIB_VERSION       "${PROJECT_VERSION}")   # used by apps/rtss-updater/CMakeLists.txt
set(RTSS_GUIDPT_LIB_SOVERSION     "1")

# Output directory for shared libraries: <build_dir>/lib/
set(RTSS_LIB_OUTPUT_DIR    ${CMAKE_CURRENT_BINARY_DIR}/lib)
file(MAKE_DIRECTORY ${RTSS_LIB_OUTPUT_DIR})

# Logging options
#
# RTSS_UMD_DEBUG_PRINT: log sink
#   ON  (default) — printf to stdout (console)
#   OFF           — syslog; view with: journalctl -t rtss-mailbox
#   Usage: cmake -DRTSS_UMD_DEBUG_PRINT=OFF ..
#
# RTSS_UMD_LOG_LEVEL: verbosity (0=none 1=err 2=warn 3=info 4=dbg, default=3)
#   Usage: cmake -DRTSS_UMD_LOG_LEVEL=1 ..
option(RTSS_UMD_DEBUG_PRINT "Print logs to stdout instead of syslog" ON)
set(RTSS_UMD_LOG_LEVEL "3" CACHE STRING "Log verbosity: 0=none 1=err 2=warn 3=info 4=dbg")

if(RTSS_UMD_DEBUG_PRINT)
    add_compile_definitions(RTSS_UMD_DEBUG_PRINT=1)
else()
    add_compile_definitions(RTSS_UMD_DEBUG_PRINT=0)
endif()

add_compile_definitions(RTSS_UMD_LOG_LEVEL=${RTSS_UMD_LOG_LEVEL})

add_subdirectory(middleware/safemlib   ${CMAKE_CURRENT_BINARY_DIR}/middleware/safemlib)
add_subdirectory(middleware/mailbox    ${CMAKE_CURRENT_BINARY_DIR}/middleware/mailbox)
add_subdirectory(apps/rtss-updater    ${CMAKE_CURRENT_BINARY_DIR}/apps/rtss-updater)
add_subdirectory(apps/rtss-dbg        ${CMAKE_CURRENT_BINARY_DIR}/apps/rtss-dbg)
add_subdirectory(apps/rtss-console    ${CMAKE_CURRENT_BINARY_DIR}/apps/rtss-console)
add_subdirectory(apps/rtss-mbdemo     ${CMAKE_CURRENT_BINARY_DIR}/apps/rtss-mbdemo)

install(FILES api/rtss_mailbox_api.h DESTINATION include)
