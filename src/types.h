// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_TYPES_H
#define JLD_MCC_TYPES_H

#include <cstdint>
#include <variant>

#include "source.h"

enum class BuiltinTypeKind : uint8_t {
  Int,
};

struct BuiltinType {
  BuiltinTypeKind kind;
};

using TypeVariant = std::variant<BuiltinType>;

struct Type {
  TypeVariant variant;
  SourceRange source;
};

#endif  // JLD_MCC_TYPES_H