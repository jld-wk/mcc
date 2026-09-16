// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_VALUES_H
#define JLD_MCC_IR_VALUES_H

#include <cstdint>
#include <variant>

#include "types.h"

struct IrValueInt {
  int value;
};

struct IrValueChar {
  char value;
};

struct IrValuePointer {
  std::uintptr_t addr;
};

struct IrValueUndefined {};

using IrValueVariant = std::variant<IrValueInt, IrValueChar, IrValuePointer, IrValueUndefined>;

struct IrValue {
  IrValueVariant variant;
  Type*          type;
};

#endif  // JLD_MCC_IR_VALUES_H