# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_console)

cmake_minimum_required(VERSION 3.10)

set(RTSS_CONSOLE_SRC_FILES
    src/rtss_console.c
)

add_executable(rtss_console ${RTSS_CONSOLE_SRC_FILES})

target_compile_options(rtss_console PRIVATE -Wall -Wno-unused-variable)

target_include_directories(rtss_console PRIVATE
    ${RTSS_API_INC_PATH}
    ${RTSS_COMMON_INC_PATH}
)

target_link_libraries(rtss_console rtss_mailbox pthread rt)

install(TARGETS rtss_console DESTINATION bin)
