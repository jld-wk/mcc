// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_VALUES_H
#define JLD_MCC_IR_VALUES_H

#include <cassert>
#include <climits>
#include <cstdint>
#include <variant>

#include "types.h"

enum class IrIntegerType : uint8_t { U1, U8, U16, U32, U64 };

constexpr auto ir_integer_type_bits(IrIntegerType type) -> uint8_t {
  return type == IrIntegerType::U1    ? 1
         : type == IrIntegerType::U8  ? 8
         : type == IrIntegerType::U16 ? 16
         : type == IrIntegerType::U32 ? 32
                                      : 64;
}

struct IrIntegerValue {
  uint64_t      val;
  uint8_t       bits;
  IrIntegerType type;
  bool          isSigned{ false };

  IrIntegerValue()
      : val{ 0 }
      , bits{ 1 }
      , type{ IrIntegerType::U8 } {}

  IrIntegerValue(uint64_t val, IrIntegerType type)
      : val{ val }
      , bits{ ir_integer_type_bits(type) }
      , type{ type } {}

  IrIntegerValue(uint64_t val, uint8_t bits, IrIntegerType type)
      : val{ val }
      , bits{ bits }
      , type{ type } {}

  [[nodiscard]] constexpr auto overflow(IrIntegerType to_type) const -> bool {
    uint64_t to_max{ to_type == IrIntegerType::U1    ? BOOL_MAX
                     : to_type == IrIntegerType::U8  ? UINT8_MAX
                     : to_type == IrIntegerType::U16 ? UINT16_MAX
                     : to_type == IrIntegerType::U16 ? UINT32_MAX
                     : to_type == IrIntegerType::U64 ? UINT64_MAX
                                                     : 0 };

    switch (type) {
      case IrIntegerType::U1:
        return true;
      case IrIntegerType::U8:
      case IrIntegerType::U16:
      case IrIntegerType::U32:
      case IrIntegerType::U64:
        return val > to_max;
    }

    assert(false);
    return true;
  }

  [[nodiscard]] constexpr auto add_overflow(IrIntegerType to_type, uint64_t add) const -> bool {
    uint64_t to_max{ to_type == IrIntegerType::U1    ? BOOL_MAX
                     : to_type == IrIntegerType::U8  ? UINT8_MAX
                     : to_type == IrIntegerType::U16 ? UINT16_MAX
                     : to_type == IrIntegerType::U32 ? UINT32_MAX
                     : to_type == IrIntegerType::U64 ? UINT64_MAX
                                                     : 0 };

    switch (type) {
      case IrIntegerType::U1:
        return true;
      case IrIntegerType::U8:
      case IrIntegerType::U16:
      case IrIntegerType::U32:
      case IrIntegerType::U64:
        return (val + add) > to_max;
    }

    assert(false);
    return true;
  }

  [[nodiscard]] constexpr auto sub_overflow(IrIntegerType to_type, uint64_t sub) const -> bool {
    uint64_t to_max{ to_type == IrIntegerType::U1    ? BOOL_MAX
                     : to_type == IrIntegerType::U8  ? UINT8_MAX
                     : to_type == IrIntegerType::U16 ? UINT16_MAX
                     : to_type == IrIntegerType::U32 ? UINT32_MAX
                     : to_type == IrIntegerType::U64 ? UINT64_MAX
                                                     : 0 };

    switch (type) {
      case IrIntegerType::U1:
        return true;
      case IrIntegerType::U8:
      case IrIntegerType::U16:
      case IrIntegerType::U32:
      case IrIntegerType::U64:
        return val > (to_max - sub);
    }

    assert(false);
    return true;
  }

  [[nodiscard]] constexpr auto mul_overflow(IrIntegerType to_type, uint64_t mul) const -> bool {
    uint64_t to_max{ to_type == IrIntegerType::U1    ? BOOL_MAX
                     : to_type == IrIntegerType::U8  ? UINT8_MAX
                     : to_type == IrIntegerType::U16 ? UINT16_MAX
                     : to_type == IrIntegerType::U32 ? UINT32_MAX
                     : to_type == IrIntegerType::U64 ? UINT64_MAX
                                                     : 0 };

    switch (type) {
      case IrIntegerType::U1:
        return true;
      case IrIntegerType::U8:
      case IrIntegerType::U16:
      case IrIntegerType::U32:
      case IrIntegerType::U64:
        uint64_t res = val * mul;
        if (res > to_max)
          return true;
        return val != 0 && res / val != mul;
    }

    assert(false);
    return true;
  }
};

struct IrPointerValue {
  std::uintptr_t addr;
};

struct IrUndeclaredValue {};

struct IrErrorValue {};

using IrValueData = std::variant<IrIntegerValue, IrPointerValue, IrErrorValue, IrUndeclaredValue>;

struct IrValue {
  IrValueData data;
  Type*       type;

  [[nodiscard]] constexpr auto is_error() const -> bool {
    return std::holds_alternative<IrErrorValue>(data);
  }
};

constexpr auto make_ir_error_value() -> IrValue {
  return IrValue{ .data = IrErrorValue{}, .type = nullptr };
}

#endif  // JLD_MCC_IR_VALUES_H