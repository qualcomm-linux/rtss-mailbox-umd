# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_ota_demo)

cmake_minimum_required(VERSION 3.10)

set(RTSS_OTA_SRC_FILES
    rtss_ota.c
)

# CMake target named rtss_ota_bin to avoid conflict with librtss_ota target.
# Output binary name stays rtss_ota via OUTPUT_NAME.
add_executable(rtss_ota_bin ${RTSS_OTA_SRC_FILES})
set_target_properties(rtss_ota_bin PROPERTIES OUTPUT_NAME rtss_ota)

target_compile_options(rtss_ota_bin PRIVATE -Wall -Wno-unused-variable)

target_include_directories(rtss_ota_bin PRIVATE
    ${RTSS_OTA_INC_PATH}
    ${RTSS_API_INC_PATH}
    ${RTSS_COMMON_INC_PATH}
    ${RTSS_UAPI_INC_PATH}
    ${SYSROOTINC_PATH}
    ${SYSROOT_INCLUDEDIR}
)

target_link_libraries(rtss_ota_bin rtss_ota rtss_mailbox pthread rt glib-2.0)

install(TARGETS rtss_ota_bin DESTINATION bin)
