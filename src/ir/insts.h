// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INSTS_H
#define JLD_MCC_IR_INSTS_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "diagnostic/source.h"

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

using InstParameter = std::variant<IrSlot, size_t>;

struct StoreInst {
  InstParameter p1;
  IrSlot        slot;
};

enum class ArithmeticInstKind : uint8_t {
  Add,
  Sub,
  Mul,
  Div,
};

struct ArithmeticInst {
  ArithmeticInstKind kind;

  InstParameter p1;
  InstParameter p2;
  IrSlot        slot;
};

enum class ComparisionInstKind : uint8_t { Eq, Ne, Lt, Le, Gt, Ge };

struct ComparisionInst {
  ComparisionInstKind kind;

  InstParameter p1;
  InstParameter p2;
  IrSlot        slot;
};

enum class BranchInstKind : uint8_t { Jmp, Call };

struct BranchInst {
  BranchInstKind kind;

  std::string_view branch;
};

struct BranchIfInst {
  BranchInstKind kind;

  InstParameter       p1;
  InstParameter       p2;
  ComparisionInstKind p3;
  std::string_view    branch;
};

struct AllocInst {
  InstParameter p1;
  IrSlot        slot;
};

struct FreeInst {
  IrSlot p1;
};

struct LoadAddrInst {
  IrSlot p1;
  IrSlot slot;
};

struct StorePtrInst {
  InstParameter p1;
  InstParameter p2;
  IrSlot        slot;
};

struct StoreAddrInst {
  IrSlot p1;
  IrSlot slot;
};

struct RetInst {
  std::vector<InstParameter> params;
};
struct ExitInst {
  InstParameter p1;
};

enum class DumpInstKind : uint8_t { Decimal, Char };

struct DumpInst {
  DumpInstKind kind;

  InstParameter p1;
};

using IrInstVariant =
    std::variant<StoreInst, ArithmeticInst, ComparisionInst, BranchInst, BranchIfInst, AllocInst,
                 FreeInst, LoadAddrInst, StorePtrInst, StoreAddrInst, RetInst, ExitInst, DumpInst>;

struct IrInst {
  IrInstVariant variant;
  SourceRange   source;
};

struct IrBranch {
  std::vector<IrInst*> insts;
  std::string_view     identifier;
  SourceRange          range;
};

struct IrFunction {
  std::vector<IrInst*>                            insts;
  std::unordered_map<std::string_view, IrBranch*> branches;
  std::vector<std::string_view>                   params;
  std::string_view                                identifier;
  SourceRange                                     range;
};

#endif  // JLD_MCC_IR_INSTS_H