// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INSTS_H
#define JLD_MCC_IR_INSTS_H

#include <cstddef>
#include <string_view>
#include <variant>
#include <vector>

#include "diagnostic/source.h"

struct PushInst {
  size_t number;
};
struct PopInst {};

struct StoreInst {
  std::string_view identifier;
};
struct LoadInst {
  std::string_view identifier;
};

struct AddInst {};
struct SubInst {};
struct MulInst {};
struct DivInst {};

struct EqInst {};
struct NeInst {};
struct LtInst {};
struct LeInst {};
struct GtInst {};
struct GeInst {};
struct JmpInst {
  std::string_view branch;
};
struct JmpTInst {
  std::string_view branch;
};
struct JmpFInst {
  std::string_view branch;
};
struct CallInst {
  std::string_view branch;
};
struct CallTInst {
  std::string_view branch;
};
struct CallFInst {
  std::string_view branch;
};

struct AllocInst {};
struct FreeInst {};
struct StoreAddrInst {
  std::string_view identifier;
};
struct LoadAddrInst {};

struct DupInst {};

struct ExitInst {};

struct DumpDInst {};
struct DumpCInst {};

using InstVariant =
    std::variant<PushInst, PopInst, StoreInst, LoadInst, AddInst, SubInst, MulInst, DivInst, EqInst,
                 NeInst, LtInst, LeInst, GtInst, GeInst, JmpInst, JmpTInst, JmpFInst, CallInst,
                 CallTInst, CallFInst, AllocInst, FreeInst, StoreAddrInst, LoadAddrInst, DupInst,
                 ExitInst, DumpDInst, DumpCInst>;

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