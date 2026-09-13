// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_DECLS_H
#define JLD_MCC_DECLS_H

#include <string_view>
#include <variant>

#include "diagnostic/source.h"
#include "stmts.h"
#include "types.h"

struct FunctionDecl {
  Type*            type;
  Stmt*            stmt;
  std::string_view identifer;
};

using DeclVariant = std::variant<FunctionDecl>;

struct Decl {
  DeclVariant variant;
  SourceRange source;
};

#endif  // JLD_MCC_DECLS_H