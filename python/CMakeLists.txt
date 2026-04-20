# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Rok Zitko

cmake_minimum_required(VERSION 3.15...3.27)
project(pp)

if (CMAKE_VERSION VERSION_LESS 3.18)
  set(DEV_MODULE Development)
else()
  set(DEV_MODULE Development.Module)
endif()

find_package(Python 3.8 COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Detect the installed nanobind package and import it into CMake
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(pp
  pp.cc
  pp_bind_misc.cc
  pp_bind_sequence.cc
  pp_bind_hw_base.cc
  pp_bind_streaming.cc
  pp_bind_signal_paths.cc
  pp_bind_counters.cc)
nanobind_add_module(pp_impl pp_impl.cc)

target_include_directories(pp
    PRIVATE ${CMAKE_SOURCE_DIR}/../c++
            ${CMAKE_SOURCE_DIR}/..
            ${CMAKE_SOURCE_DIR}/../tidbit
            ${CMAKE_SOURCE_DIR}/../c++/third_party/hwlib/include
            ${CMAKE_SOURCE_DIR}/../c++/third_party/hwlib/include/soc_cv_av
            /usr/include
)

target_include_directories(pp_impl
    PRIVATE ${CMAKE_SOURCE_DIR}/../c++
            ${CMAKE_SOURCE_DIR}/..
            ${CMAKE_SOURCE_DIR}/../tidbit
            ${CMAKE_SOURCE_DIR}/../c++/third_party/hwlib/include
            ${CMAKE_SOURCE_DIR}/../c++/third_party/hwlib/include/soc_cv_av
            /usr/include
)
