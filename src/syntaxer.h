// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_SYNTAXER_H
#define JLD_MCC_SYNTAXER_H

#include <cassert>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

#include "arena.h"
#include "decls.h"
#include "diagnostic/source.h"
#include "exprs.h"
#include "stmts.h"
#include "tokenizer.h"
#include "type_arena.h"
#include "types.h"
#include "utility.h"

class Syntaxer {
 public:
  explicit Syntaxer(const std::vector<Token>& tokens, TypeArena& types)
      : m_types_{ types }
      , m_tokens_{ tokens } {}

  [[nodiscard]] auto build() -> std::vector<Decl*> {
    std::vector<Decl*> decls;
    while (current().kind != TokenKind::EndOfFile) {
      Decl* decl = build_decl();
      assert(decl != nullptr);
      decls.push_back(decl);
    }
    return decls;
  }

 private:
  [[nodiscard]] auto build_expr() -> Expr* {
    const Token& start = current();
    if (match(TokenKind::Number)) {
      std::string_view view = start.text;
      int              literal{ 0 };
      std::from_chars(view.data(), view.data() + view.length(), literal);
      return m_exprs_.emplace(IntLiteralExpr{ .literal = literal }, start.range);
    }
    // TODO(jld-wk): print diagnostic
    return nullptr;
  }

  [[nodiscard]] auto build_block() -> Stmt* {
    const Token& start = current();
    if (!expect(TokenKind::OpenBrace)) {
      // TODO(jld-wk): recovery stuff}
    }

    std::vector<BlockItem> items;
    while (!peek(TokenKind::CloseBrace) && !peek(TokenKind::EndOfFile)) {
      BlockItem item{ .ptr = build_decl(), .kind = BlockItemKind::Declaration };
      if (item.ptr == nullptr) {
        item.ptr = build_stmt();
        item.kind = item.ptr == nullptr ? BlockItemKind::Undefined : BlockItemKind::Statement;
      }
      items.push_back(item);
    }

    const Token& end = current();
    expect(TokenKind::CloseBrace);
    return m_stmts_.emplace(BlockStmt{ .items = std::move(items) },
                            source_multi_range_from(start.range, end.range));
  }

  [[nodiscard]] auto build_stmt(bool w_block = true) -> Stmt* {
    if (w_block && peek(TokenKind::OpenBrace))
      return build_block();
    const Token& start = current();
    if (match(TokenKind::KywReturn)) {
      Expr* expr = build_expr();
      expect(TokenKind::Semicolon);
      return m_stmts_.emplace(
          ReturnStmt{
              .expr = expr,
          },
          source_multi_range_from(start.range, start.range));
    }
    // TODO(jld-wk): build stmts
    return nullptr;
  }

  [[nodiscard]] auto build_type() -> Type* {
    [[maybe_unused]] const Token& cur = current();
    if (match(TokenKind::KywInt)) {
      return m_types_.emplace(BuiltinType{ .kind = BuiltinTypeKind::U32 });
    }

    // TODO(jld-wk): print diagnostic
    return nullptr;
  }

  [[nodiscard]] auto build_decl() -> Decl* {
    Type* type = build_type();
    if (type != nullptr) {
      const Token& identifier = current();
      if (!expect(TokenKind::Identifier)) {
        // TODO(jld-wk): recovery stuff
      }
      if (!expect(TokenKind::OpenParen)) {
      }
      if (!expect(TokenKind::CloseParen)) {
      }

      Stmt* stmt = nullptr;

      if (match(TokenKind::Equal))
        stmt = build_stmt(false);
      else
        stmt = build_block();

      if (stmt == nullptr) {
        // TODO(jld-wk): recovery stuff
      }

      return m_decls_.emplace(
          FunctionDecl{
              .type = type,
              .stmt = stmt,
              .identifer = identifier.text,
          },
          identifier.range);
    }

    return nullptr;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto current() -> const Token& {
    return m_tokens_[m_curToken_];
  }

  JLD_MCC_FORCE_INLINE void advance_cur() {
    m_curToken_ += 1;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto advance() -> const Token& {
    const Token& cur = current();
    advance_cur();
    return cur;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto peek(TokenKind kind) -> bool {
    return current().kind == kind;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto match(TokenKind kind) -> bool {
    const Token& cur = current();
    if (cur.kind == kind) {
      advance_cur();
      return true;
    }
    return false;
  }

  JLD_MCC_FORCE_INLINE auto expect(TokenKind kind) -> bool {
    bool matches = match(kind);
    if (!matches) {
      // TODO(jld-wk) print diagnostic
    }
    return matches;
  }

 private:
  Arena<Expr> m_exprs_;
  Arena<Stmt> m_stmts_;
  Arena<Decl> m_decls_;

  TypeArena& m_types_;

  const std::vector<Token>& m_tokens_;
  size_t                    m_curToken_{ 0 };
};

#endif  // JLD_MCC_SYNTAXER_H