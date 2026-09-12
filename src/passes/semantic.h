// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_SEMANTIC_PASS_H
#define JLD_MCC_SEMANTIC_PASS_H

#include <cassert>
#include <print>
#include <variant>
#include <vector>

#include "decls.h"
#include "exprs.h"
#include "stmts.h"
#include "type_arena.h"
#include "types.h"
#include "utility.h"

/*

TODOs:
* Name resolution
* Scopes
* Type resolution (typedef etc.)
* Type checking (advanced)

int main() {
  return 42;
}

For the current project, main should turn into a function symbol, which then replaces the function
declaration (so might be an syntaxer step already). Then the entry scope of main is entered and
therfore the current function symbol is main. Return should just query the latest entered function
and compare the function and expression type.

*/

template <bool Debug>
class SemanticPass {
 public:
  explicit SemanticPass(TypeArena& types)
      : m_types_{ types } {}

  void analyze(std::vector<Decl*>& ast) {
    for (Decl* decl : ast) analyze_decl(decl);
  }

 private:
  auto compare_types(Type* type_a, Type* type_b) -> bool {
    assert(type_a != nullptr && type_b != nullptr);
    return type_a->variant == type_b->variant;
  }

  void analyze_expr(Expr* p_expr) {
    std::visit(Overload{ [&](const IntLiteralExpr& expr) -> void {
                 if constexpr (Debug) {
                   std::println("Analyzing Integer Literal Expression -> {}", expr.literal);
                 }

                 p_expr->type = m_types_.query(BuiltinType{ .kind = BuiltinTypeKind::Int });
               } },
               p_expr->variant);
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
                           assert(compare_types(
                               stmt.expr->type,
                               m_types_.query(BuiltinType{ .kind = BuiltinTypeKind::Int })));
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

 private:
  TypeArena& m_types_;
};

#endif