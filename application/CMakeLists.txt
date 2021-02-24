#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.13.1)

list(APPEND ZEPHYR_EXTRA_MODULES
   ${CMAKE_CURRENT_SOURCE_DIR}/../spm_module)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

zephyr_include_directories(
  kernel/include
  ${ARCH_DIR}/${ARCH}/include
)
# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/measures.c)
zephyr_sources(src/secure_services_ns.c)
  # NORDIC SDK APP END
zephyr_include_directories(src)
