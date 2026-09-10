# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_update)

cmake_minimum_required(VERSION 3.10)

set(RTSS_UPDATE_INC_PATH ${CMAKE_CURRENT_LIST_DIR}/include)
set(RTSS_UPDATE_INC_PATH ${RTSS_UPDATE_INC_PATH} PARENT_SCOPE)

set(RTSS_UPDATE_LIB_SRC_FILES
    src/rtss_update_lib.c
)

add_library(rtss_update SHARED ${RTSS_UPDATE_LIB_SRC_FILES})
add_library(rtss_update_static STATIC ${RTSS_UPDATE_LIB_SRC_FILES})
set_target_properties(rtss_update_static PROPERTIES OUTPUT_NAME rtss_updater)
set_target_properties(rtss_update PROPERTIES
    VERSION   ${RTSS_UPDATE_LIB_VERSION}
    SOVERSION ${RTSS_UPDATE_LIB_SOVERSION}
    LIBRARY_OUTPUT_DIRECTORY ${RTSS_LIB_OUTPUT_DIR}
)

foreach(tgt rtss_update rtss_update_static)
    target_compile_options(${tgt} PRIVATE -Wall -Wno-unused-variable)
    target_include_directories(${tgt} PRIVATE
        ${RTSS_UPDATE_INC_PATH}
        ${RTSS_API_INC_PATH}
        ${RTSS_COMMON_INC_PATH}
    )
    target_link_libraries(${tgt} rtss_mailbox pthread rt c)
endforeach()

install(FILES ${RTSS_UPDATE_INC_PATH}/rtss_update_lib.h DESTINATION include)
install(TARGETS rtss_update rtss_update_static
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
