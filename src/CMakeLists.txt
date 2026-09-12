# SPDX-FileCopyrightText: (c) 2026 Julian Duwe
# SPDX-License-Identifier: Apache-2.0

add_executable(mcc main.cpp)

include_directories(mcc PRIVATE "./")

target_compile_options(mcc PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Werror
    -Wnrvo
)