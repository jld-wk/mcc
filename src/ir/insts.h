// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INSTS_H
#define JLD_MCC_IR_INSTS_H

#include <cassert>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "diagnostic/source.h"
#include "ir/values.h"
#include "types.h"

enum class IrSlotType : uint8_t {
  Local,
  Global,
  Register,
  Return,
  Argument,
  Undefined,
};

struct IrUntypedSlot {
  std::string_view ident;
  SourceRange      identRange;
  // SourceRange   typeRange; -> argRange
  IrSlotType type{ IrSlotType::Undefined };

  [[nodiscard]] constexpr auto format_type() const -> const char* {
    switch (type) {
      case IrSlotType::Local:
        return "<local>";
      case IrSlotType::Global:
        return "g";
      case IrSlotType::Register:
        return "r";
      case IrSlotType::Return:
        return "ret";
      case IrSlotType::Argument:
        return "arg";
      case IrSlotType::Undefined:
        return "<undefined>";
    }

    assert(false);
    return "<unknown>";
  }
};

struct IrSlot {
  IrUntypedSlot slot;
  SourceRange   typeRange;
};

using InstArgData = std::variant<IrUntypedSlot, IrValue>;

struct InstArg {
  InstArgData data;
  SourceRange argRange;
};

struct StoreInst {
  InstArg arg0;
  IrSlot  dest;
};

enum class IrArithmeticOpKind : uint8_t {
  Add,
  Sub,
  Mul,
  Div,
};

struct ArithmeticInst {
  InstArg arg0;
  InstArg arg1;
  IrSlot  dest;

  IrArithmeticOpKind kind;
};

enum class IrComparisionOpKind : uint8_t { Eq, Ne, Lt, Le, Gt, Ge, Undefined };

struct ComparisionInst {
  InstArg arg0;
  InstArg arg1;
  IrSlot  dest;

  IrComparisionOpKind kind;
};

struct JumpInst {
  std::string_view dest;
  SourceRange      destRange;
};

struct CallInst {
  std::string_view dest;
  SourceRange      destRange;
};

enum class BranchIfInstKind : uint8_t {
  Jump,
  Call,
};

struct BranchIfInst {
  InstArg arg0;
  InstArg arg2;

  std::string_view dest;
  SourceRange      destRange;

  BranchIfInstKind    kind;
  IrComparisionOpKind arg1;
};

struct AllocInst {
  IrSlot  dest;
  InstArg arg0;
};

struct FreeInst {
  IrSlot arg0;
};

struct LoadAddrInst {
  IrSlot dest;
  IrSlot arg0;
};

struct StoreAtInst {
  InstArg arg0;
  InstArg dest1;
  IrSlot  dest;
};

struct StoreAddrInst {
  IrSlot arg0;
  IrSlot dest;
};

struct RetInst {
  std::vector<InstArg> args;
};

struct ExitInst {
  InstArg arg0;
};

struct PrintInst {
  InstArg arg0;
};

struct DeclareInst {
  std::string_view ident;
  SourceMultiRange typeRange;
  Type*            type{ nullptr };
};

using IrInstData = std::variant<StoreInst, ArithmeticInst, ComparisionInst, JumpInst, CallInst,
                                BranchIfInst, AllocInst, FreeInst, LoadAddrInst, StoreAtInst,
                                StoreAddrInst, RetInst, ExitInst, PrintInst, DeclareInst>;

struct IrInst {
  IrInstData  data;
  SourceRange instRange;
};

struct IrLabel {
  std::vector<IrInst*> insts;
  std::string_view     ident;
  SourceRange          identRange;
};

struct IrFunctionParam {
  std::string_view ident;
  SourceMultiRange typeRange;
  SourceRange      identRange;
  Type*            type{ nullptr };
};

struct IrFunctionScope;

struct IrFunction {
  std::unordered_map<std::string_view, IrLabel*> labels;
  std::vector<IrInst*>                           insts;
  std::vector<IrFunctionParam>                   params;
  std::string_view                               ident;
  SourceRange                                    identRange;
  IrFunctionScope*                               scope;
};

#endif  // JLD_MCC_IR_INSTS_H