// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_STMTS_H
#define JLD_MCC_STMTS_H

#include <cstdint>
#include <variant>
#include <vector>

#include "exprs.h"
#include "source.h"

enum class BlockItemKind : uint8_t { Undefined, Statement, Declaration };

struct BlockItem {
  void*         ptr{ nullptr };
  BlockItemKind kind{ BlockItemKind::Undefined };
};

struct BlockStmt {
  std::vector<BlockItem> items;
};

struct ReturnStmt {
  Expr* expr{ nullptr };
};

using StmtVariant = std::variant<BlockStmt, ReturnStmt>;

struct Stmt {
  StmtVariant variant;
  SourceRange source;
};

#endif  // JLD_MCC_STMTS_H