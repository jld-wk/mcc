// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_SYNTAXER_H
#define JLD_MCC_IR_SYNTAXER_H

#include <cassert>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <vector>

#include "arena.h"
#include "diagnostic/source.h"
#include "insts.h"
#include "ir_tokenizer.h"
#include "utility.h"

class IrSyntaxer {
 public:
  explicit IrSyntaxer(const std::vector<IrToken>& tokens)
      : m_tokens_{ tokens } {}

  // TODO(jld-wk): don't actually need an vector, just use the arena directly, BUT remind me when
  // the token arena is done...
  [[nodiscard]] auto build() -> std::vector<Branch*> {
    std::vector<Branch*> branches;
    while (current().kind != IrTokenKind::EndOfFile) {
      Branch* branch{ build_branch() };
      assert(branch != nullptr);
      branches.push_back(branch);
    }
    return branches;
  }

 private:
  [[nodiscard]] auto build_number(bool* ok) -> int {
    const IrToken& cur{ current() };
    if (!expect(IrTokenKind::Number)) {
      *ok = false;
      return 0;
    }

    std::string_view view{ cur.text };
    int              number{ 0 };
    std::from_chars(view.data(), view.data() + view.length(), number);
    return number;
  }

  [[nodiscard]] auto build_inst() -> Inst* {
    const IrToken& start{ current() };

    if (match(IrTokenKind::KywPush)) {
      const IrToken& end{ current() };
      bool           ok{ true };
      int            number{ build_number(&ok) };
      if (!ok) {
        // TODO(jld-wk): thats not okay!
      }
      return m_insts_.emplace(PushInst{ .number = number },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywPop))
      return m_insts_.emplace(PopInst{}, start.source);

    if (match(IrTokenKind::KywStore)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(StoreInst{ .identifier = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywLoad)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(LoadInst{ .identifier = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywAdd))
      return m_insts_.emplace(AddInst{}, start.source);
    if (match(IrTokenKind::KywSub))
      return m_insts_.emplace(SubInst{}, start.source);
    if (match(IrTokenKind::KywMul))
      return m_insts_.emplace(MulInst{}, start.source);
    if (match(IrTokenKind::KywDiv))
      return m_insts_.emplace(DivInst{}, start.source);

    if (match(IrTokenKind::KywEq))
      return m_insts_.emplace(EqInst{}, start.source);
    if (match(IrTokenKind::KywNe))
      return m_insts_.emplace(NeInst{}, start.source);
    if (match(IrTokenKind::KywLt))
      return m_insts_.emplace(LtInst{}, start.source);
    if (match(IrTokenKind::KywLe))
      return m_insts_.emplace(LeInst{}, start.source);
    if (match(IrTokenKind::KywGt))
      return m_insts_.emplace(GtInst{}, start.source);
    if (match(IrTokenKind::KywGe))
      return m_insts_.emplace(GeInst{}, start.source);

    if (match(IrTokenKind::KywDup))
      return m_insts_.emplace(DupInst{}, start.source);
    if (match(IrTokenKind::KywExit))
      return m_insts_.emplace(ExitInst{}, start.source);

    if (match(IrTokenKind::KywCall)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(CallInst{ .branch = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywCallT)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(CallTInst{ .branch = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywCallF)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(CallFInst{ .branch = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmp)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(JmpInst{ .branch = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmpT)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(JmpTInst{ .branch = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmpF)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(JmpFInst{ .branch = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywDbgDump))
      return m_insts_.emplace(DbgDumpInst{}, start.source);

    // TODO(jld-wk): print diagnostic
    return nullptr;
  }

  [[nodiscard]] auto build_branch() -> Branch* {
    const IrToken& start{ current() };
    if (!expect(IrTokenKind::Identifier)) {
      // TODO(jld-wk): recovery stuff!!
    }
    if (!expect(IrTokenKind::Colon)) {
      // same thing... again..
    }

    std::vector<Inst*> insts;
    while (!peek(IrTokenKind::Identifier) && !peek(IrTokenKind::EndOfFile))
      insts.push_back(build_inst());

    const IrToken& end{ previous() };
    return m_branches_.emplace(insts, start.text, source_range_from(start.source, end.source));
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto current() -> const IrToken& {
    return m_tokens_[m_curToken_];
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto previous() -> const IrToken& {
    return m_tokens_[m_curToken_ - 1];
  }

  JLD_MCC_FORCE_INLINE void advance_cur() {
    m_curToken_ += 1;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto advance() -> const IrToken& {
    const IrToken& cur{ current() };
    advance_cur();
    return cur;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto peek(IrTokenKind kind) -> bool {
    return current().kind == kind;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto match(IrTokenKind kind) -> bool {
    const IrToken& cur = current();
    if (cur.kind == kind) {
      advance_cur();
      return true;
    }
    return false;
  }

  JLD_MCC_FORCE_INLINE auto expect(IrTokenKind kind) -> bool {
    bool matches{ match(kind) };
    if (!matches) {
      // TODO(jld-wk) print diagnostic
    }
    return matches;
  }

 private:
  Arena<Inst>   m_insts_;
  Arena<Branch> m_branches_;

  const std::vector<IrToken>& m_tokens_;
  size_t                      m_curToken_{ 0 };
};

#endif  // JLD_MCC_IR_SYNTAXER_H