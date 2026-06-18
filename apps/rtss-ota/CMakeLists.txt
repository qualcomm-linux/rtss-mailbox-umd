# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss-ota)

cmake_minimum_required(VERSION 3.10)

set(RTSS_OTA_INC_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/include)

set(RTSS_OTA_LIB_SRC_FILES
    lib/src/rtss_ota_lib.c
)

add_library(rtss_ota SHARED ${RTSS_OTA_LIB_SRC_FILES})
add_library(rtss_ota_static STATIC ${RTSS_OTA_LIB_SRC_FILES})
set_target_properties(rtss_ota_static PROPERTIES OUTPUT_NAME rtss_ota)
set_target_properties(rtss_ota PROPERTIES
    VERSION   ${RTSS_OTA_LIB_VERSION}
    SOVERSION ${RTSS_OTA_LIB_SOVERSION}
    LIBRARY_OUTPUT_DIRECTORY ${RTSS_LIB_OUTPUT_DIR}
)

foreach(tgt rtss_ota rtss_ota_static)
    target_compile_options(${tgt} PRIVATE -Wall -Wno-unused-variable)
    target_include_directories(${tgt} PRIVATE
        ${RTSS_OTA_INC_PATH}
        ${RTSS_API_INC_PATH}
        ${RTSS_COMMON_INC_PATH}
        ${RTSS_UAPI_INC_PATH}
        ${SYSROOTINC_PATH}
        ${SYSROOT_INCLUDEDIR}
    )
    target_link_libraries(${tgt} rtss_mailbox pthread rt c)
endforeach()

install(FILES ${RTSS_OTA_INC_PATH}/rtss_ota_lib.h DESTINATION include)
install(TARGETS rtss_ota rtss_ota_static
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

add_subdirectory(./demo)
