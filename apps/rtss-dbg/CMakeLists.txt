# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_dbg)

cmake_minimum_required(VERSION 3.10)

set(RTSS_DBG_SRC_FILES
    rtss_dbg.c
)

add_executable(rtss_dbg ${RTSS_DBG_SRC_FILES})

target_compile_options(rtss_dbg PRIVATE -Wall -Wno-unused-variable)

target_include_directories(rtss_dbg PRIVATE
    ${RTSS_API_INC_PATH}
    ${RTSS_COMMON_INC_PATH}
)

target_link_libraries(rtss_dbg rtss_mailbox pthread rt)

install(TARGETS rtss_dbg DESTINATION bin)
