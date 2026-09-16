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
#include "ir/values.h"
#include "ir_tokenizer.h"
#include "type_arena.h"
#include "types.h"
#include "utility.h"

class IrSyntaxer {
 public:
  explicit IrSyntaxer(const std::vector<IrToken>& tokens, TypeArena& types)
      : m_types_{ types }
      , m_tokens_{ tokens } {}

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
  [[nodiscard]] auto build_int(int* output, bool expected = true) -> bool {
    const IrToken& cur{ current() };
    if (match(IrTokenKind::Integer)) {
      std::string_view view{ cur.text };
      std::from_chars(view.data(), view.data() + view.length(), *output);
      return true;
    }

    if (expected)
      expect(IrTokenKind::Integer);
    return false;
  }

  [[nodiscard]] auto build_slot(IrSlot* slot, bool expected = true) -> bool {
    const IrToken& cur{ current() };
    if (match(IrTokenKind::Slot)) {
      size_t n = 0;
      while (cur.text.length() > n && cur.text[n] != ':') ++n;

      std::string_view type{ cur.text.substr(0, n) };
      std::string_view identifier{ n == cur.text.length()
                                       ? cur.text
                                       : cur.text.substr(n + 1, cur.text.length()) };

      IrSlotKind kind = IrSlotKind::Local;
      if (type == "r")
        kind = IrSlotKind::Register;
      else if (type == "g")
        kind = IrSlotKind::Global;
      else if (type == "arg")
        kind = IrSlotKind::Argument;
      else if (type == "ret")
        kind = IrSlotKind::Return;
      else if (n != cur.text.length()) {
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

  [[nodiscard]] auto build_inst_arg(InstArg* param) -> bool {
    int integer{ 0 };
    if (build_int(&integer, false)) {
      *param = IrValue{ .variant = IrValueInt{ .value = integer },
                        .type = m_types_.emplace(BuiltinType{ .kind = BuiltinTypeKind::Int }) };
      return true;
    }

    const IrToken& cur{ current() };
    if (match(IrTokenKind::Char)) {
      *param = IrValue{ .variant = IrValueChar{ .value = cur.text[0] },
                        .type = m_types_.emplace(BuiltinType{ .kind = BuiltinTypeKind::Char }) };
      return true;
    }

    IrSlot slot;
    bool   ok = build_slot(&slot, true);
    *param = slot;
    return ok;
  }

  [[nodiscard]] auto build_type() -> Type* {
    Type*                           cur_type = nullptr;
    [[maybe_unused]] const IrToken& cur = current();

    if (match(IrTokenKind::KywInt))
      cur_type = m_types_.emplace(BuiltinType{ .kind = BuiltinTypeKind::Int });
    else if (match(IrTokenKind::KywChar))
      cur_type = m_types_.emplace(BuiltinType{ .kind = BuiltinTypeKind::Char });

    if (cur_type == nullptr) {
      // print diagnostic
    }

    while (match(IrTokenKind::Star))
      cur_type = m_types_.emplace(PointerType{ .pointee = cur_type });

    return cur_type;
  }

  [[nodiscard]] auto comparision_kind(IrTokenKind kind) -> ComparisionInstKind {
    return kind == IrTokenKind::KywEq   ? ComparisionInstKind::Eq
           : kind == IrTokenKind::KywNe ? ComparisionInstKind::Ne
           : kind == IrTokenKind::KywLt ? ComparisionInstKind::Lt
           : kind == IrTokenKind::KywLe ? ComparisionInstKind::Le
           : kind == IrTokenKind::KywGt ? ComparisionInstKind::Gt
           : kind == IrTokenKind::KywGe ? ComparisionInstKind::Ge
                                        : ComparisionInstKind::Undefined;
  }

  [[nodiscard]] auto build_inst() -> IrInst* {
    const IrToken& start{ current() };

    if (match(IrTokenKind::KywStore)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest;
      bool   idk_unused = build_slot(&dest);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(StoreInst{ .arg0 = arg0, .dest = dest },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywAdd) || match(IrTokenKind::KywSub) || match(IrTokenKind::KywMul) ||
        match(IrTokenKind::KywDiv)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      InstArg arg1;
      bool    idk_unused = build_inst_arg(&arg1);
      _ = idk_unused;

      expect(IrTokenKind::Arrow);

      IrSlot dest;
      bool   well_obv_unused = build_slot(&dest);
      idk_unused = well_obv_unused;
      const IrToken& end{ previous() };

      ArithmeticInstKind kind = start.kind == IrTokenKind::KywAdd   ? ArithmeticInstKind::Add
                                : start.kind == IrTokenKind::KywSub ? ArithmeticInstKind::Sub
                                : start.kind == IrTokenKind::KywMul ? ArithmeticInstKind::Mul
                                                                    : ArithmeticInstKind::Div;
      return m_insts_.emplace(
          ArithmeticInst{ .kind = kind, .arg0 = arg0, .arg1 = arg1, .dest = dest },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywEq) || match(IrTokenKind::KywNe) || match(IrTokenKind::KywLt) ||
        match(IrTokenKind::KywLe) || match(IrTokenKind::KywGt) || match(IrTokenKind::KywGe)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      InstArg arg1;
      bool    idk_unused = build_inst_arg(&arg1);
      _ = idk_unused;

      expect(IrTokenKind::Arrow);

      IrSlot dest;
      bool   well_obv_unused = build_slot(&dest);
      idk_unused = well_obv_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(
          ComparisionInst{
              .kind = comparision_kind(start.kind), .arg0 = arg0, .arg1 = arg1, .dest = dest },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmp) || match(IrTokenKind::KywCall)) {
      expect(IrTokenKind::Identifier);
      const IrToken& end{ previous() };

      return m_insts_.emplace(
          BranchInst{ .kind = start.kind == IrTokenKind::KywJmp ? BranchInstKind::Jmp
                                                                : BranchInstKind::Call,
                      .dest = end.text },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywJmpIf) || match(IrTokenKind::KywCallIf)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      ComparisionInstKind arg1 = comparision_kind(advance().kind);

      InstArg arg2;
      bool    idk_unused = build_inst_arg(&arg2);
      _ = idk_unused;

      expect(IrTokenKind::Arrow);

      expect(IrTokenKind::Identifier);
      const IrToken& end{ previous() };

      return m_insts_.emplace(
          BranchIfInst{ .kind = start.kind == IrTokenKind::KywJmpIf ? BranchInstKind::Jmp
                                                                    : BranchInstKind::Call,
                        .arg0 = arg0,
                        .arg1 = arg1,
                        .arg2 = arg2,
                        .dest = end.text },
          source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywAlloc)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest;
      bool   idk_unused = build_slot(&dest);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(AllocInst{ .arg0 = arg0, .dest = dest },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywFree)) {
      IrSlot         arg0;
      bool           _ = build_slot(&arg0);
      const IrToken& end{ previous() };

      return m_insts_.emplace(FreeInst{ .arg0 = arg0 },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywLoadAddr)) {
      IrSlot arg0;
      bool   _ = build_slot(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest;
      bool   idk_unused = build_slot(&dest);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(LoadAddrInst{ .arg0 = arg0, .dest = dest },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywStoreAt)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest;
      bool   idk_unused = build_slot(&dest);
      _ = idk_unused;

      InstArg offset;
      bool    well_obv_unused = build_inst_arg(&offset);
      idk_unused = well_obv_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(StoreAtInst{ .arg0 = arg0, .dest = dest, .offset = offset },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywStoreAddr)) {
      IrSlot arg0;
      bool   _ = build_slot(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest;
      bool   idk_unused = build_slot(&dest);
      _ = idk_unused;
      const IrToken& end{ previous() };

      return m_insts_.emplace(StoreAddrInst{ .arg0 = arg0, .dest = dest },
                              source_range_from(start.source, end.source));
    }

    if (match(IrTokenKind::KywRet)) {
      std::vector<InstArg> values;
      if (!match(IrTokenKind::KywVoid)) {
        do {
          InstArg val;
          bool    _ = build_inst_arg(&val);
          values.push_back(val);
        } while (match(IrTokenKind::Comma) && !peek(IrTokenKind::EndOfFile));
      } else {
        // errorrrrr
      }
      return m_insts_.emplace(RetInst{ .values = std::move(values) }, start.source);
    }

    if (match(IrTokenKind::KywExit)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);
      return m_insts_.emplace(ExitInst{ .arg0 = arg0 }, start.source);
    }

    if (match(IrTokenKind::KywDump)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);
      return m_insts_.emplace(DumpInst{ .arg0 = arg0 }, start.source);
    }

    if (match(IrTokenKind::Identifier)) {
      expect(IrTokenKind::Colon);
      Type* type = build_type();
      return m_insts_.emplace(DeclareInst{ .identifier = start.text, .type = type }, start.source);
    }

    assert(false);
    // TODO(jld-wk): print diagnostic
    return nullptr;
  }

  [[nodiscard]] auto build_label() -> IrLabel* {
    const IrToken& start{ current() };
    if (!expect(IrTokenKind::Dot)) {
      // TODO(jld-wk): recovery stuff!!
    }
    const IrToken& ident{ current() };
    if (!expect(IrTokenKind::Identifier)) {
      // TODO(jld-wk): recovery stuff!!
    }
    if (!expect(IrTokenKind::Colon)) {
      // same thing... again..
    }

    std::vector<IrInst*> insts;
    while (!peek(IrTokenKind::KywEnd) && !peek(IrTokenKind::Dot) && !peek(IrTokenKind::EndOfFile)) {
      IrInst* inst = build_inst();
      insts.push_back(inst);
    }

    // TODO(jld-wk): Require end to be either a jmp, jmp_if or ret
    const IrToken& end{ previous() };

    return m_branches_.emplace(insts, ident.text, source_range_from(start.source, end.source));
  }

  [[nodiscard]] auto build_function() -> IrFunction* {
    const IrToken& start{ current() };
    if (!expect(IrTokenKind::Identifier)) {
      // TODO(jld-wk): recovery stuff!!
    }
    if (!expect(IrTokenKind::OpenParen)) {
      // same thing... again..
    }

    std::vector<IrFunctionParameter> params;
    if (peek(IrTokenKind::Identifier)) {
      do {
        const IrToken& cur = current();
        expect(IrTokenKind::Identifier);
        expect(IrTokenKind::Colon);
        Type* type = build_type();
        params.emplace_back(cur.text, type);
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

    std::vector<IrInst*>                           insts;
    std::unordered_map<std::string_view, IrLabel*> labels;

    while (!peek(IrTokenKind::KywEnd) && !peek(IrTokenKind::EndOfFile)) {
      if (peek(IrTokenKind::Dot)) {
        IrLabel* label = build_label();
        labels[label->identifier] = label;
        continue;
      }

      IrInst* inst = build_inst();
      insts.push_back(inst);
    }

    expect(IrTokenKind::KywEnd);
    const IrToken& end{ previous() };

    return m_functions_.emplace(insts, labels, params, start.text,
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
  TypeArena&        m_types_;
  Arena<IrInst>     m_insts_;
  Arena<IrLabel>    m_branches_;
  Arena<IrFunction> m_functions_;

  const std::vector<IrToken>& m_tokens_;
  size_t                      m_curToken_{ 0 };
};

#endif  // JLD_MCC_IR_SYNTAXER_H