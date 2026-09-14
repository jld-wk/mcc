// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_SYNTAXER_H
#define JLD_MCC_IR_SYNTAXER_H

#include <cassert>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <utility>
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
  [[nodiscard]] auto build() -> std::vector<IrFunction*> {
    std::vector<IrFunction*> functions;
    while (current().kind != IrTokenKind::EndOfFile) {
      IrFunction* function{ build_function() };
      assert(function != nullptr);
      functions.push_back(function);
    }
    return functions;
  }

 private:
  [[nodiscard]] auto build_number(size_t* number, bool expected = true) -> bool {
    const IrToken& cur{ current() };
    if (match(IrTokenKind::Number)) {
      std::string_view view{ cur.text };
      std::from_chars(view.data(), view.data() + view.length(), *number);
      return true;
    }

    if (expected)
      expect(IrTokenKind::Number);
    return false;
  }

  [[nodiscard]] auto build_slot(IrSlot* slot, bool expected = true) -> bool {
    const IrToken& cur{ current() };
    if (match(IrTokenKind::Slot)) {
      std::string_view view{ cur.text.substr(1, cur.text.length() - 2) };

      size_t n = 0;
      while (view.length() > n && view[n] != ':') ++n;

      std::string_view type{ view.substr(0, n) };
      std::string_view identifier{ n == view.length() ? view : view.substr(n + 1, view.length()) };

      IrSlotKind kind = IrSlotKind::Local;
      if (type == "r")
        kind = IrSlotKind::Register;
      else if (type == "g")
        kind = IrSlotKind::Global;
      else if (type == "arg")
        kind = IrSlotKind::Argument;
      else if (type == "ret")
        kind = IrSlotKind::Return;
      else if (n != view.length()) {
        assert(false);
        // TODO(jld-wk): unknown type -> error
      }

      *slot = IrSlot{
        .kind = kind,
        .identifier = identifier,
      };
      return true;
    }

    if (expected)
      expect(IrTokenKind::Slot);
    return false;
  }

  [[nodiscard]] auto build_inst_paramter(InstParameter* param) -> bool {
    size_t number{ 0 };
    if (build_number(&number, false)) {
      *param = number;
      return true;
    }

    IrSlot slot;
    bool   ok = build_slot(&slot, true);
    *param = slot;
    return ok;
  }

  [[nodiscard]] auto comparision_kind(IrTokenKind kind) -> ComparisionInstKind {
    return kind == IrTokenKind::KywEq   ? ComparisionInstKind::Eq
           : kind == IrTokenKind::KywNe ? ComparisionInstKind::Ne
           : kind == IrTokenKind::KywLt ? ComparisionInstKind::Lt
           : kind == IrTokenKind::KywLe ? ComparisionInstKind::Le
           : kind == IrTokenKind::KywGt ? ComparisionInstKind::Gt
                                        : ComparisionInstKind::Ge;
  }

  [[nodiscard]] auto build_inst() -> IrInst* {
    const IrToken& start{ current() };

    if (match(IrTokenKind::KywStore)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);
      expect(IrTokenKind::Comma);

      IrSlot slot;
      bool   idk_unused = build_slot(&slot);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(StoreInst{ .p1 = p1, .slot = slot },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywAdd) || match(IrTokenKind::KywSub) || match(IrTokenKind::KywMul) ||
        match(IrTokenKind::KywDiv)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);

      expect(IrTokenKind::Comma);
      InstParameter p2;
      bool          idk_unused = build_inst_paramter(&p2);
      _ = idk_unused;
      expect(IrTokenKind::Comma);

      IrSlot slot;
      bool   well_obv_unused = build_slot(&slot);
      idk_unused = well_obv_unused;
      const IrToken& end{ previous() };

      ArithmeticInstKind kind = start.kind == IrTokenKind::KywAdd   ? ArithmeticInstKind::Add
                                : start.kind == IrTokenKind::KywSub ? ArithmeticInstKind::Sub
                                : start.kind == IrTokenKind::KywMul ? ArithmeticInstKind::Mul
                                                                    : ArithmeticInstKind::Div;
      return m_insts_.emplace(ArithmeticInst{ .kind = kind, .p1 = p1, .p2 = p2, .slot = slot },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywEq) || match(IrTokenKind::KywNe) || match(IrTokenKind::KywLt) ||
        match(IrTokenKind::KywLe) || match(IrTokenKind::KywGt) || match(IrTokenKind::KywGe)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);

      expect(IrTokenKind::Comma);
      InstParameter p2;
      bool          idk_unused = build_inst_paramter(&p2);
      _ = idk_unused;
      expect(IrTokenKind::Comma);

      IrSlot slot;
      bool   well_obv_unused = build_slot(&slot);
      idk_unused = well_obv_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(
          ComparisionInst{ .kind = comparision_kind(start.kind), .p1 = p1, .p2 = p2, .slot = slot },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmp) || match(IrTokenKind::KywCall)) {
      expect(IrTokenKind::Identifier);
      const IrToken& end{ previous() };

      return m_insts_.emplace(
          BranchInst{ .kind = start.kind == IrTokenKind::KywJmp ? BranchInstKind::Jmp
                                                                : BranchInstKind::Call,
                      .branch = end.text },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmpIf) || match(IrTokenKind::KywCallIf)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);

      expect(IrTokenKind::Comma);
      InstParameter p2;
      bool          idk_unused = build_inst_paramter(&p2);
      _ = idk_unused;
      expect(IrTokenKind::Comma);

      ComparisionInstKind p3 = comparision_kind(advance().kind);
      expect(IrTokenKind::Comma);

      expect(IrTokenKind::Identifier);
      const IrToken& end{ previous() };

      return m_insts_.emplace(
          BranchIfInst{ .kind = start.kind == IrTokenKind::KywJmpIf ? BranchInstKind::Jmp
                                                                    : BranchInstKind::Call,
                        .p1 = p1,
                        .p2 = p2,
                        .p3 = p3,
                        .branch = end.text },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywAlloc)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);
      expect(IrTokenKind::Comma);

      IrSlot slot;
      bool   idk_unused = build_slot(&slot);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(AllocInst{ .p1 = p1, .slot = slot },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywFree)) {
      IrSlot         p1;
      bool           _ = build_slot(&p1);
      const IrToken& end{ previous() };

      return m_insts_.emplace(FreeInst{ .p1 = p1 }, source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywLoadAddr)) {
      IrSlot p1;
      bool   _ = build_slot(&p1);
      expect(IrTokenKind::Comma);

      IrSlot slot;
      bool   idk_unused = build_slot(&slot);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(LoadAddrInst{ .p1 = p1, .slot = slot },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywStorePtr)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);
      expect(IrTokenKind::Comma);

      InstParameter p2;
      bool          idk_unused = build_inst_paramter(&p2);
      _ = idk_unused;
      expect(IrTokenKind::Comma);

      IrSlot slot;
      bool   well_obv_unused = build_slot(&slot);
      idk_unused = well_obv_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(StorePtrInst{ .p1 = p1, .p2 = p2, .slot = slot },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywStoreAddr)) {
      IrSlot p1;
      bool   _ = build_slot(&p1);
      expect(IrTokenKind::Comma);

      IrSlot slot;
      bool   idk_unused = build_slot(&slot);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(LoadAddrInst{ .p1 = p1, .slot = slot },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywRet)) {
      std::vector<InstParameter> params;
      if (!match(IrTokenKind::KywVoid)) {
        do {
          InstParameter param;
          bool          _ = build_inst_paramter(&param);
          params.push_back(param);
        } while (match(IrTokenKind::Comma) && !peek(IrTokenKind::EndOfFile));
      } else {
        // errorrrrr
      }
      return m_insts_.emplace(RetInst{ .params = std::move(params) }, start.source);
    }

    if (match(IrTokenKind::KywExit)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);
      return m_insts_.emplace(ExitInst{ .p1 = p1 }, start.source);
    }

    if (match(IrTokenKind::KywDumpD) || match(IrTokenKind::KywDumpC)) {
      InstParameter p1;
      bool          _ = build_inst_paramter(&p1);
      return m_insts_.emplace(
          DumpInst{ .kind = start.kind == IrTokenKind::KywDumpD ? DumpInstKind::Decimal
                                                                : DumpInstKind::Char,
                    .p1 = p1 },
          start.source);
    }

    assert(false);
    // TODO(jld-wk): print diagnostic
    return nullptr;
  }

  [[nodiscard]] auto build_branch() -> IrBranch* {
    const IrToken& start{ current() };
    if (!expect(IrTokenKind::Identifier)) {
      // TODO(jld-wk): recovery stuff!!
    }
    if (!expect(IrTokenKind::Colon)) {
      // same thing... again..
    }

    std::vector<IrInst*> insts;
    while (!peek(IrTokenKind::KywEnd) && !peek(IrTokenKind::EndOfFile)) {
      IrInst* inst = build_inst();
      insts.push_back(inst);
    }

    expect(IrTokenKind::KywEnd);
    const IrToken& end{ previous() };

    return m_branches_.emplace(insts, start.text, source_range_from(start.source, end.source));
  }

  [[nodiscard]] auto build_function() -> IrFunction* {
    const IrToken& start{ current() };
    if (!expect(IrTokenKind::Identifier)) {
      // TODO(jld-wk): recovery stuff!!
    }
    if (!expect(IrTokenKind::OpenParen)) {
      // same thing... again..
    }

    std::vector<std::string_view> params;
    if (peek(IrTokenKind::Identifier)) {
      do {
        const IrToken& cur = current();
        expect(IrTokenKind::Identifier);
        params.push_back(cur.text);
      } while (match(IrTokenKind::Comma) && !peek(IrTokenKind::EndOfFile));
    } else if (!expect(IrTokenKind::KywVoid)) {
      // same thing... again..
    }

    if (!expect(IrTokenKind::CloseParen)) {
      // same thing... again..
    }
    if (!expect(IrTokenKind::Colon)) {
      // same thing... again..
    }
    if (!expect(IrTokenKind::Minus)) {
      // same thing... again..
    }

    std::vector<IrInst*>                            insts;
    std::unordered_map<std::string_view, IrBranch*> branches;

    while (!peek(IrTokenKind::Minus) && !peek(IrTokenKind::EndOfFile)) {
      if (peek(IrTokenKind::Identifier)) {
        IrBranch* branch = build_branch();
        branches[branch->identifier] = branch;
        continue;
      }

      IrInst* inst = build_inst();
      insts.push_back(inst);
    }

    const IrToken& end{ current() };
    expect(IrTokenKind::Minus);

    return m_functions_.emplace(insts, branches, params, start.text,
                                source_range_from(start.source, end.source));
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
  Arena<IrInst>     m_insts_;
  Arena<IrBranch>   m_branches_;
  Arena<IrFunction> m_functions_;

  const std::vector<IrToken>& m_tokens_;
  size_t                      m_curToken_{ 0 };
};

#endif  // JLD_MCC_IR_SYNTAXER_H