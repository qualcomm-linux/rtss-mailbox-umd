# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_mailbox)

cmake_minimum_required(VERSION 3.10)

set(RTSS_MBOX_SRC_FILES
    src/rtss_mailbox.c
)

add_library(rtss_mailbox SHARED ${RTSS_MBOX_SRC_FILES})
add_library(rtss_mailbox_static STATIC ${RTSS_MBOX_SRC_FILES})
set_target_properties(rtss_mailbox_static PROPERTIES OUTPUT_NAME rtss_mailbox)
set_target_properties(rtss_mailbox PROPERTIES
    VERSION   ${RTSS_MB_LIB_VERSION}
    SOVERSION ${RTSS_MB_LIB_SOVERSION}
)

foreach(tgt rtss_mailbox rtss_mailbox_static)
    target_compile_options(${tgt} PRIVATE -Wall -Wno-unused-variable)
    target_include_directories(${tgt} PRIVATE
        ${RTSS_API_INC_PATH}
        ${RTSS_COMMON_INC_PATH}
        ${RTSS_SAFEM_INC_PATH}
        ${RTSS_UAPI_INC_PATH}
        ${SYSROOTINC_PATH}
        ${SYSROOT_INCLUDEDIR}
    )
    target_link_libraries(${tgt} rtss_safemlib pthread rt)
endforeach()

install(TARGETS rtss_mailbox rtss_mailbox_static
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
