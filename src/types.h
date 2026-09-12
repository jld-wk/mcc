// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_TYPES_H
#define JLD_MCC_TYPES_H

#include <cstdint>
#include <variant>

enum class BuiltinTypeKind : uint8_t {
  Int,
};

struct BuiltinType {
  BuiltinTypeKind kind;

  auto operator==(BuiltinType other) const -> bool {
    return kind == other.kind;
  }
};

using TypeVariant = std::variant<BuiltinType>;

struct Type {
  TypeVariant variant;
};

#endif  // JLD_MCC_TYPES_H