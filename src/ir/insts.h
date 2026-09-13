// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INSTS_H
#define JLD_MCC_IR_INSTS_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

#include "diagnostic/source.h"

using InstParameter = std::variant<std::string_view, size_t>;

enum class StoreInstKind : uint8_t { Local, Ret, Param };

struct StoreInst {
  StoreInstKind kind;

  InstParameter    a;
  std::string_view toVar;
};

enum class LoadInstKind : uint8_t { Local, Ret, Param };

struct LoadInst {
  LoadInstKind kind;

  std::string_view a;
  std::string_view toVar;
};

enum class ArithmeticInstKind : uint8_t {
  Add,
  Sub,
  Mul,
  Div,
};

struct ArithmeticInst {
  ArithmeticInstKind kind;

  InstParameter    a;
  InstParameter    b;
  std::string_view toVar;
};

enum class ComparisionInstKind : uint8_t { Eq, Ne, Lt, Le, Gt, Ge };

struct ComparisionInst {
  ComparisionInstKind kind;

  InstParameter    a;
  InstParameter    b;
  std::string_view toVar;
};

enum class BranchInstKind : uint8_t { Jmp, Call };

struct BranchInst {
  BranchInstKind kind;

  std::string_view toBranch;
};

struct BranchIfInst {
  BranchInstKind kind;

  InstParameter       a;
  InstParameter       b;
  ComparisionInstKind cmpKind;
  std::string_view    toBranch;
};

struct AllocInst {
  InstParameter    a;
  std::string_view toVar;
};

struct FreeInst {
  std::string_view identifier;
};

struct LoadAddrInst {
  std::string_view a;
  std::string_view toVar;
};

struct StorePtrInst {
  InstParameter    a;
  InstParameter    b;
  std::string_view toVar;
};

struct StoreAddrInst {
  std::string_view a;
  std::string_view toVar;
};

struct ExitInst {
  InstParameter a;
};

enum class DumpInstKind : uint8_t { Decimal, Char };

struct DumpInst {
  DumpInstKind kind;

  InstParameter a;
};

using InstVariant = std::variant<StoreInst, LoadInst, ArithmeticInst, ComparisionInst, BranchInst,
                                 BranchIfInst, AllocInst, FreeInst, LoadAddrInst, StorePtrInst,
                                 StoreAddrInst, ExitInst, DumpInst>;

struct Inst {
  InstVariant variant;
  SourceRange source;
};

struct Branch {
  std::vector<Inst*> insts;
  std::string_view   identifier;
  SourceRange        range;
};

#endif  // JLD_MCC_IR_INSTS_H