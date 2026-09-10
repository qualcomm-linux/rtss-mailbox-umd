# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_safemlib)

cmake_minimum_required(VERSION 3.10)

set(RTSS_MBOX_SRC_FILES
    src/rtss_mailbox_lib.c
)

add_library(rtss_safemlib SHARED ${RTSS_MBOX_SRC_FILES})
add_library(rtss_safemlib_static STATIC ${RTSS_MBOX_SRC_FILES})
set_target_properties(rtss_safemlib_static PROPERTIES OUTPUT_NAME rtss_safemlib)
set_target_properties(rtss_safemlib PROPERTIES
    VERSION   ${RTSS_MB_LIB_VERSION}
    SOVERSION ${RTSS_MB_LIB_SOVERSION}
)

foreach(tgt rtss_safemlib rtss_safemlib_static)
    target_compile_options(${tgt} PRIVATE -Wall -Wno-unused-variable)
    target_include_directories(${tgt} PRIVATE
        ${RTSS_SAFEM_INC_PATH}
    )
    target_link_libraries(${tgt} pthread rt)
endforeach()

install(TARGETS rtss_safemlib rtss_safemlib_static
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
