// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INSTS_H
#define JLD_MCC_IR_INSTS_H

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "diagnostic/source.h"
#include "ir/values.h"
#include "types.h"

enum class IrSlotKind : uint8_t {
  Local,
  Global,
  Register,
  Return,
  Argument,
};

struct IrSlot {
  IrSlotKind       kind{ IrSlotKind::Local };
  std::string_view identifier;
};

using InstArg = std::variant<IrSlot, IrValue>;

struct StoreInst {
  InstArg arg0;
  IrSlot  dest;
};

enum class ArithmeticInstKind : uint8_t {
  Add,
  Sub,
  Mul,
  Div,
};

struct ArithmeticInst {
  ArithmeticInstKind kind;

  InstArg arg0;
  InstArg arg1;
  IrSlot  dest;
};

enum class ComparisionInstKind : uint8_t { Eq, Ne, Lt, Le, Gt, Ge, Undefined };

struct ComparisionInst {
  ComparisionInstKind kind;

  InstArg arg0;
  InstArg arg1;
  IrSlot  dest;
};

enum class BranchInstKind : uint8_t { Jmp, Call };

struct BranchInst {
  BranchInstKind kind;

  std::string_view dest;
};

struct BranchIfInst {
  BranchInstKind kind;

  InstArg             arg0;
  ComparisionInstKind arg1;
  InstArg             arg2;
  std::string_view    dest;
};

struct AllocInst {
  InstArg arg0;
  IrSlot  dest;
};

struct FreeInst {
  IrSlot arg0;
};

struct LoadAddrInst {
  IrSlot arg0;
  IrSlot dest;
};

struct StoreAtInst {
  InstArg arg0;
  IrSlot  dest;
  InstArg offset;
};

struct StoreAddrInst {
  IrSlot arg0;
  IrSlot dest;
};

struct RetInst {
  std::vector<InstArg> values;
};

struct ExitInst {
  InstArg arg0;
};

struct DumpInst {
  InstArg arg0;
};

struct DeclareInst {
  std::string_view identifier;
  Type*            type{ nullptr };
};

using IrInstVariant = std::variant<StoreInst, ArithmeticInst, ComparisionInst, BranchInst,
                                   BranchIfInst, AllocInst, FreeInst, LoadAddrInst, StoreAtInst,
                                   StoreAddrInst, RetInst, ExitInst, DumpInst, DeclareInst>;

struct IrInst {
  IrInstVariant variant;
  SourceRange   source;
};

struct IrLabel {
  std::vector<IrInst*> insts;
  std::string_view     identifier;
  SourceRange          range;
};

struct IrFunctionParameter {
  std::string_view identifier;
  Type*            type{ nullptr };
};

struct IrFunction {
  std::vector<IrInst*>                           insts;
  std::unordered_map<std::string_view, IrLabel*> labels;
  std::vector<IrFunctionParameter>               params;
  std::string_view                               identifier;
  SourceRange                                    range;
};

#endif  // JLD_MCC_IR_INSTS_H