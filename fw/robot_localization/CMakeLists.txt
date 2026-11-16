# SPDX-License-Identifier: GPL-3.0-only

# Compatibility entry point for the previous package.
cmake_minimum_required(VERSION 3.16)
project(robot_navigation_compat LANGUAGES C CXX)
include(CTest)
add_subdirectory(.. navigation)
