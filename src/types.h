// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_TYPES_H
#define JLD_MCC_TYPES_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

#include "utility.h"

enum class BuiltinTypeKind : uint8_t {
  Error,

  U32,
  U8,
};

[[nodiscard]] auto format_builtin_type_kind(BuiltinTypeKind kind) -> const char* {
  switch (kind) {
    case BuiltinTypeKind::Error:
      return "error";

    case BuiltinTypeKind::U32:
      return "int";
    case BuiltinTypeKind::U8:
      return "char";
  }

  assert(false);
  return "?";
}

struct BuiltinType {
  BuiltinTypeKind kind;

  [[nodiscard]] auto bytes_size() const -> size_t {
    switch (kind) {
      case BuiltinTypeKind::Error:
        return 1;
      case BuiltinTypeKind::U32:
        return 4;
      case BuiltinTypeKind::U8:
        return 1;
    }
    return 0;
  }

  [[nodiscard]] auto is_integer() const -> bool {
    return kind == BuiltinTypeKind::U8 || kind == BuiltinTypeKind::U32;
  }

  [[nodiscard]] auto potentially_negative() const -> bool {
    return kind == BuiltinTypeKind::U8 || kind == BuiltinTypeKind::U32;
  }

  [[nodiscard]] auto operator==(BuiltinType other) const -> bool {
    return kind == other.kind;
  }
};

class Type;

struct PointerType {
  Type* pointee{ nullptr };

  [[nodiscard]] auto operator==(PointerType other) const -> bool {
    return pointee == other.pointee;
  }
};

struct VoidPointerType {
  uint32_t depth{ 0 };

  [[nodiscard]] auto operator==(VoidPointerType other) const -> bool {
    return depth == other.depth;
  }
};

using TypeData = std::variant<BuiltinType, PointerType>;

class Type {
 public:
  TypeData data;

  [[nodiscard]] constexpr auto format() -> std::string {
    return std::visit(
        Overload{
            [](BuiltinType type) -> std::string { return format_builtin_type_kind(type.kind); },
            [](PointerType type) -> std::string { return type.pointee->format() + "*"; },
        },
        data);
  }

  [[nodiscard]] constexpr auto bytes_size() -> size_t {
    return std::visit(Overload{
                          [](BuiltinType type) -> size_t { return type.bytes_size(); },
                          // TODO(jld-wk): respect 32-bit systems :(
                          [](PointerType) -> size_t { return 8; },
                      },
                      data);
  }

  [[nodiscard]] constexpr auto is_ptr() const -> size_t {
    const auto* ptr = std::get_if<PointerType>(&data);
    return ptr == nullptr ? 0 : ptr->pointee->bytes_size();
  }

  [[nodiscard]] constexpr auto is_ptr_to_ptr() const -> bool {
    const auto* ptr = std::get_if<PointerType>(&data);
    return ptr != nullptr && std::holds_alternative<PointerType>(ptr->pointee->data);
  }

  [[nodiscard]] constexpr auto is_ptr_to_builtin() const -> bool {
    const auto* ptr = std::get_if<PointerType>(&data);
    return ptr != nullptr && std::holds_alternative<BuiltinType>(ptr->pointee->data);
  }

  [[nodiscard]] constexpr auto is_integer() const -> bool {
    const auto* builtin = std::get_if<BuiltinType>(&data);
    return builtin != nullptr && builtin->is_integer();
  }

  [[nodiscard]] constexpr auto potentially_negative() const -> bool {
    const auto* builtin = std::get_if<BuiltinType>(&data);
    return builtin != nullptr && builtin->potentially_negative();
  }
};

#endif  // JLD_MCC_TYPES_H