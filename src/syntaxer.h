// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_SYNTAXER_H
#define JLD_MCC_SYNTAXER_H

#include <cassert>
#include <cstddef>
#include <vector>

#include "tokenize.h"
#include "utility.h"

class Syntaxer {
 public:
  explicit Syntaxer(const std::vector<Token>& tokens)
      : m_tokens_{ tokens } {}

  auto build() -> void* {
    assert(match(TokenKind::KywInt));
    return nullptr;
  }

 private:
  JLD_MCC_FORCE_INLINE [[nodiscard]] auto current() -> const Token& {
    return m_tokens_[m_curToken_];
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto advance_cur() {
    m_curToken_ += 1;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto advance() -> const Token& {
    const Token& cur = current();
    advance_cur();
    return cur;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto match(TokenKind kind) -> bool {
    const Token& cur = current();
    if (cur.kind == kind) {
      advance_cur();
      return true;
    }
    return false;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto expect(TokenKind kind) -> bool {
    bool matches = match(kind);
    if (!matches) {
      // TODO(jld-wk) print diagnostic
    }
    return matches;
  }

 private:
  const std::vector<Token>& m_tokens_;
  size_t                    m_curToken_{ 0 };
};

#endif  // JLD_MCC_SYNTAXER