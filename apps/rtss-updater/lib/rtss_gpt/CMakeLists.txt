# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss_gpt)

cmake_minimum_required(VERSION 3.10)

set(RTSS_GUIDPT_INC_PATH ${CMAKE_CURRENT_LIST_DIR}/include)
set(RTSS_GUIDPT_INC_PATH ${RTSS_GUIDPT_INC_PATH} PARENT_SCOPE)

set(RTSS_GPT_LIB_SRC_FILES
    src/rtss_guidpt.c
)

add_library(rtss_gpt SHARED ${RTSS_GPT_LIB_SRC_FILES})
add_library(rtss_gpt_static STATIC ${RTSS_GPT_LIB_SRC_FILES})
set_target_properties(rtss_gpt_static PROPERTIES OUTPUT_NAME rtss_gpt)
set_target_properties(rtss_gpt PROPERTIES
    VERSION   ${RTSS_GUIDPT_LIB_VERSION}
    SOVERSION ${RTSS_GUIDPT_LIB_SOVERSION}
    LIBRARY_OUTPUT_DIRECTORY ${RTSS_LIB_OUTPUT_DIR}
)

foreach(tgt rtss_gpt rtss_gpt_static)
    target_compile_options(${tgt} PRIVATE -Wall -Wno-unused-variable)
    target_include_directories(${tgt} PRIVATE
        ${RTSS_GUIDPT_INC_PATH}
        ${RTSS_COMMON_INC_PATH}
    )
endforeach()

install(FILES ${RTSS_GUIDPT_INC_PATH}/rtss_guidpt.h DESTINATION include)
install(TARGETS rtss_gpt rtss_gpt_static
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
