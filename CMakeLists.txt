# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Pull in the out-of-tree RGB indicator driver module (LP5817 driver + DT binding)
list(APPEND EXTRA_ZEPHYR_MODULES "C:/NCS/loouq/modules/rgb-indicator")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rgbindicator VERSION 1.0.0 DESCRIPTION "RGB indicator using TI LED controller")

set(LOOUQ_BASE "C:/NCS/loouq")
message(STATUS "Zephyr at: ${ZEPHYR_BASE}")
message(STATUS "LooUQ at: ${LOOUQ_BASE}")

# *** extract all CMAKE variables to output ***
# get_cmake_property(all_vars VARIABLES)
# foreach(var ${all_vars})
#     message(STATUS "VAR: ${var}=${${var}}")
# endforeach()

target_sources(app PRIVATE src/main.c)
