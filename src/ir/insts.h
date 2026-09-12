// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INSTS_H
#define JLD_MCC_IR_INSTS_H

#include <string_view>
#include <variant>
#include <vector>

#include "source.h"

struct PushInst {
  int number{ 0 };
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

struct RetInst {};

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

struct DbgDumpInst {};

using InstVariant =
    std::variant<PushInst, PopInst, StoreInst, LoadInst, AddInst, SubInst, MulInst, DivInst,
                 RetInst, JmpInst, JmpTInst, JmpFInst, CallInst, CallTInst, CallFInst, DbgDumpInst>;

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