// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_SEMANTIC_PASS_H
#define JLD_MCC_SEMANTIC_PASS_H

#include <print>
#include <variant>
#include <vector>

#include "decls.h"
#include "exprs.h"
#include "stmts.h"
#include "utility.h"

template <bool Debug>
class SemanticPass {
 public:
  void analyze(std::vector<Decl*>& ast) {
    for (Decl* decl : ast) analyze_decl(decl);
  }

 private:
  void analyze_expr(Expr* expr) {
    std::visit(Overload{ [&](const IntLiteralExpr& expr) -> void {
                 if constexpr (Debug) {
                   std::println("Analyzing Integer Literal Expression -> {}", expr.literal);
                 }
               } },
               expr->variant);
  }

  void analyze_stmt(Stmt* stmt) {
    std::visit(Overload{ [&](const BlockStmt& stmt) -> void {
                          if constexpr (Debug) {
                            std::println("Analyzing Block Statement");
                          }

                          for (BlockItem item : stmt.items) {
                            if (item.kind == BlockItemKind::Statement)
                              analyze_stmt(static_cast<Stmt*>(item.ptr));
                            else if (item.kind == BlockItemKind::Declaration)
                              analyze_decl(static_cast<Decl*>(item.ptr));
                          }
                        },
                         [&](const ReturnStmt& stmt) -> void {
                           if constexpr (Debug) {
                             std::println("Analyzing Return Statement");
                           }

                           analyze_expr(stmt.expr);
                         } },
               stmt->variant);
  }

  void analyze_decl(Decl* decl) {
    std::visit(Overload{ [&](const FunctionDecl& decl) -> void {
                 if constexpr (Debug) {
                   std::println("Analyzing Function Declaration -> {}", decl.identifer);
                 }

                 analyze_stmt(decl.stmt);
               } },
               decl->variant);
  }
};

#endif