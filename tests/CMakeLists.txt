# SPDX-FileCopyrightText: (c) 2026 Julian Duwe
# SPDX-License-Identifier: Apache-2.0

add_executable(arena_test
  arena_test.cpp
)

target_compile_options(arena_test PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Werror
    -Wnrvo
)

add_test(
  NAME arena
  COMMAND arena_test
)

include_directories(arena_test PRIVATE ../src)