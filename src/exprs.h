// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_EXPRS_H
#define JLD_MCC_EXPRS_H

#include <variant>

#include "diagnostic/source.h"
#include "types.h"

struct IntLiteralExpr {
  int literal{ 0 };
};

using ExprVariant = std::variant<IntLiteralExpr>;

struct Expr {
  ExprVariant variant;
  SourceRange source;
  Type*       type{ nullptr };
};

#endif  // JLD_MCC_EXPRS_H