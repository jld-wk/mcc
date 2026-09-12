# SPDX-FileCopyrightText: (c) 2026 Julian Duwe
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.27)
project(mcc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

