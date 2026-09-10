# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_updater_demo)

cmake_minimum_required(VERSION 3.10)

set(RTSS_UPDATER_SRC_FILES
    src/rtss_updater.c
    src/rtss_updater_cmd.c
)

add_executable(rtss_updater ${RTSS_UPDATER_SRC_FILES})

target_compile_options(rtss_updater PRIVATE -Wall -Wno-unused-variable)

target_include_directories(rtss_updater PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${RTSS_UPDATE_INC_PATH}
    ${RTSS_GUIDPT_INC_PATH}
    ${RTSS_API_INC_PATH}
    ${RTSS_COMMON_INC_PATH}
)

target_link_libraries(rtss_updater rtss_update rtss_gpt rtss_mailbox pthread rt glib-2.0)

install(TARGETS rtss_updater DESTINATION bin)
