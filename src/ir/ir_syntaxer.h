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
  [[nodiscard]] auto build_number(bool* ok) -> size_t {
    const IrToken& cur{ current() };
    if (!expect(IrTokenKind::Number)) {
      *ok = false;
      return 0;
    }

    std::string_view view{ cur.text };
    size_t           number{ 0 };
    std::from_chars(view.data(), view.data() + view.length(), number);
    return number;
  }

  [[nodiscard]] auto build_inst_paramter(bool* ok) -> InstParameter {
    const IrToken& cur{ current() };
    if (match(IrTokenKind::Number)) {
      std::string_view view{ cur.text };
      size_t           number{ 0 };
      std::from_chars(view.data(), view.data() + view.length(), number);
      return number;
    }

    *ok = expect(IrTokenKind::Identifier);
    return cur.text;
  }

  [[nodiscard]] auto comparision_kind(IrTokenKind kind) -> ComparisionInstKind {
    return kind == IrTokenKind::KywEq   ? ComparisionInstKind::Eq
           : kind == IrTokenKind::KywNe ? ComparisionInstKind::Ne
           : kind == IrTokenKind::KywLt ? ComparisionInstKind::Lt
           : kind == IrTokenKind::KywLe ? ComparisionInstKind::Le
           : kind == IrTokenKind::KywGt ? ComparisionInstKind::Gt
                                        : ComparisionInstKind::Ge;
  }

  [[nodiscard]] auto build_inst() -> Inst* {
    const IrToken& start{ current() };

    if (match(IrTokenKind::KywStore) || match(IrTokenKind::KywStoreRet) ||
        match(IrTokenKind::KywStoreParam)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);

      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      StoreInstKind kind = start.kind == IrTokenKind::KywStore      ? StoreInstKind::Local
                           : start.kind == IrTokenKind::KywStoreRet ? StoreInstKind::Ret
                                                                    : StoreInstKind::Param;
      return m_insts_.emplace(StoreInst{ .kind = kind, .a = a, .toVar = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywLoad) || match(IrTokenKind::KywLoadRet) ||
        match(IrTokenKind::KywLoadParam)) {
      const IrToken& a{ current() };
      expect(IrTokenKind::Identifier);
      expect(IrTokenKind::Comma);

      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      LoadInstKind kind = start.kind == IrTokenKind::KywLoad      ? LoadInstKind::Local
                          : start.kind == IrTokenKind::KywLoadRet ? LoadInstKind::Ret
                                                                  : LoadInstKind::Param;
      return m_insts_.emplace(LoadInst{ .kind = kind, .a = a.text, .toVar = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywAdd) || match(IrTokenKind::KywSub) || match(IrTokenKind::KywMul) ||
        match(IrTokenKind::KywDiv)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);
      InstParameter b = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);

      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      ArithmeticInstKind kind = start.kind == IrTokenKind::KywAdd   ? ArithmeticInstKind::Add
                                : start.kind == IrTokenKind::KywSub ? ArithmeticInstKind::Sub
                                : start.kind == IrTokenKind::KywMul ? ArithmeticInstKind::Mul
                                                                    : ArithmeticInstKind::Div;
      return m_insts_.emplace(ArithmeticInst{ .kind = kind, .a = a, .b = b, .toVar = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywEq) || match(IrTokenKind::KywNe) || match(IrTokenKind::KywLt) ||
        match(IrTokenKind::KywLe) || match(IrTokenKind::KywGt) || match(IrTokenKind::KywGe)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);
      InstParameter b = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);

      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(
          ComparisionInst{
              .kind = comparision_kind(start.kind), .a = a, .b = b, .toVar = end.text },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmp) || match(IrTokenKind::KywCall)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(
          BranchInst{ .kind = start.kind == IrTokenKind::KywJmp ? BranchInstKind::Jmp
                                                                : BranchInstKind::Call,
                      .toBranch = end.text },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmpIf) || match(IrTokenKind::KywCallIf)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);
      InstParameter b = build_inst_paramter(&ok);

      expect(IrTokenKind::Comma);
      ComparisionInstKind cmp_kind = comparision_kind(advance().kind);
      expect(IrTokenKind::Comma);

      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(
          BranchIfInst{ .kind = start.kind == IrTokenKind::KywJmpIf ? BranchInstKind::Jmp
                                                                    : BranchInstKind::Call,
                        .a = a,
                        .b = b,
                        .cmpKind = cmp_kind,
                        .toBranch = end.text },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywAlloc)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(AllocInst{ .a = a, .toVar = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywFree)) {
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);
      return m_insts_.emplace(FreeInst{ .identifier = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywLoadAddr)) {
      const IrToken& a{ current() };
      expect(IrTokenKind::Identifier);
      expect(IrTokenKind::Comma);
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(LoadAddrInst{ .a = a.text, .toVar = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywStorePtr)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);
      InstParameter b = build_inst_paramter(&ok);
      expect(IrTokenKind::Comma);

      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(StorePtrInst{ .a = a, .b = b, .toVar = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywStoreAddr)) {
      const IrToken& a{ current() };
      expect(IrTokenKind::Identifier);
      expect(IrTokenKind::Comma);
      const IrToken& end{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(StoreAddrInst{ .a = a.text, .toVar = end.text },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywExit)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);
      return m_insts_.emplace(ExitInst{ .a = a }, start.source);
    }

    if (match(IrTokenKind::KywDumpD) || match(IrTokenKind::KywDumpC)) {
      bool          ok = true;
      InstParameter a = build_inst_paramter(&ok);

      return m_insts_.emplace(
          DumpInst{ .kind = start.kind == IrTokenKind::KywDumpD ? DumpInstKind::Decimal
                                                                : DumpInstKind::Char,
                    .a = a },
          start.source);
    }

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

    bool _ = match(IrTokenKind::Minus);

    std::vector<Inst*> insts;
    while (!peek(IrTokenKind::Minus) && !peek(IrTokenKind::EndOfFile)) {
      Inst* inst = build_inst();
      insts.push_back(inst);
    }

    const IrToken& end{ advance() };
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
      assert(false);
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