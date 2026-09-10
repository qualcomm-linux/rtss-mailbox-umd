# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_mbdemo)

cmake_minimum_required(VERSION 3.10)

set(RTSS_MBDEMO_SRC_FILES
    rtss_mbdemo.c
)

add_executable(rtss_mbdemo ${RTSS_MBDEMO_SRC_FILES})

target_compile_options(rtss_mbdemo PRIVATE -Wall -Wno-unused-variable)

target_include_directories(rtss_mbdemo PRIVATE
    ${RTSS_API_INC_PATH}
    ${RTSS_COMMON_INC_PATH}
)

target_link_libraries(rtss_mbdemo rtss_mailbox pthread rt)

install(TARGETS rtss_mbdemo DESTINATION bin)
