# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtssdbg)

cmake_minimum_required(VERSION 3.10)

set(RTSSDBG_SRC_FILES
    rtssdbg.c
)

add_executable(rtssdbg ${RTSSDBG_SRC_FILES})

target_compile_options(rtssdbg PRIVATE -Wall -Wno-unused-variable)

target_include_directories(rtssdbg PRIVATE
    ${RTSS_API_INC_PATH}
    ${RTSS_COMMON_INC_PATH}
    ${RTSS_UAPI_INC_PATH}
    ${SYSROOTINC_PATH}
    ${SYSROOT_INCLUDEDIR}
)

target_link_libraries(rtssdbg rtss_mailbox pthread rt)

install(TARGETS rtssdbg DESTINATION bin)
