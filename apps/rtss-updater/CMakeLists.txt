# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#

project(rtss-updater)

cmake_minimum_required(VERSION 3.10)

add_subdirectory(./lib/rtss_update)
add_subdirectory(./lib/rtss_gpt)
add_subdirectory(./demo)
