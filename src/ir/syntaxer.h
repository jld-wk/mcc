// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_SYNTAXER_H
#define JLD_MCC_IR_SYNTAXER_H

#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "arena.h"
#include "diagnostic/core.h"
#include "diagnostic/source.h"
#include "insts.h"
#include "tokenizer.h"
#include "type_arena.h"
#include "types.h"
#include "utility.h"
#include "values.h"

/*
TODOs:
- At least a warning about unreachable code
- Expect recoveries
- More helpful errors
- Warnings

*/

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
  [[nodiscard]] auto build_number() -> int {
    const IrToken& start{ current() };
    if (match(IrTokenKind::Number)) {
      int              number{ 0 };
      std::string_view view{ start.text };
      std::from_chars(view.data(), view.data() + view.length(), number);

      return number;
    }

    return 0;
  }

  [[nodiscard]] auto build_slot() -> IrSlot {
    const IrToken& start{ current() };

    if (match(IrTokenKind::Slot)) {
      size_t n{ 0 };
      while (start.text.size() > n && start.text[n] != ':') ++n;

      bool has_type = (n != start.text.size());

      std::string_view type{ start.text.substr(0, n) };
      std::string_view ident{ has_type ? start.text.substr(n + 1, start.text.size()) : start.text };

      IrSlotType kind{ IrSlotType::Local };

      if (type == "r")
        kind = IrSlotType::Register;
      else if (type == "g")
        kind = IrSlotType::Global;
      else if (type == "arg")
        kind = IrSlotType::Argument;
      else if (type == "ret")
        kind = IrSlotType::Return;
      else if (has_type) {
        Diagnostics::report(Diagnostic{
            .severity = DiagnosticSeverity::Error,
            .range = static_cast<SourceMultiRange>(start.range),
            .message = std::format(
                "Unexpected slot type '{}', expected either 'r', 'g', 'arg' or 'ret'", type),
            .notes = {},
        });

        kind = IrSlotType::Undefined;
      }

      return IrSlot{
        .slot =
            IrUntypedSlot{
                .ident = ident,
                .identRange =
                    SourceRange{
                        .line = start.range.line,
                        .column = static_cast<uint32_t>(has_type ? (start.range.column + n + 1)
                                                                 : start.range.column),
                        .length = static_cast<uint32_t>(ident.size()),
                    },
                .type = kind,
            },
        .typeRange =
            SourceRange{
                .line = start.range.line,
                .column = start.range.column,
                .length = static_cast<uint32_t>(type.size()),
            },
      };
    }

    return IrSlot{
      .slot =
          IrUntypedSlot{
              .ident = "",
              .identRange = SourceRange{},
              .type = IrSlotType::Undefined,
          },
      .typeRange = SourceRange{},
    };
  }

  [[nodiscard]] auto build_inst_arg(InstArg* param) -> bool {
    const IrToken& start{ current() };

    if (match(IrTokenKind::Number)) {
      uint32_t         number{ 0 };
      std::string_view view{ start.text };
      std::from_chars(view.data(), view.data() + view.length(), number);

      *param = InstArg{
        .data =
            IrValue{
                .data = IrInterpIntVal{ number, IrIntegerType::U32 },
                .type = m_types_.emplace(BuiltinType{
                    .kind = BuiltinTypeKind::U32,
                }),
            },
        .argRange = start.range,
      };

      return true;
    }

    if (match(IrTokenKind::Char)) {
      *param = InstArg{
        .data =
            IrValue{
                .data =
                    IrInterpIntVal{
                        static_cast<unsigned char>(start.text[0]),
                        IrIntegerType::U8,
                    },
                .type = m_types_.emplace(BuiltinType{
                    .kind = BuiltinTypeKind::U8,
                }),
            },
        .argRange = start.range,
      };

      return true;
    }

    IrSlot slot = build_slot();
    *param = InstArg{
      .data = slot.slot,
      .argRange = slot.typeRange,
    };

    if (slot.slot.type == IrSlotType::Undefined) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(start.range),
          .message = std::format("Expected an instruction argument (slot or literal), found {}",
                                 format_ir_token_kind(start.kind)),
          .notes = {},
      });

      return false;
    }

    return true;
  }

  [[nodiscard]] auto build_type() -> Type* {
    Type*                           type = nullptr;
    [[maybe_unused]] const IrToken& start = current();

    if (match(IrTokenKind::KywU32))
      type = m_types_.emplace(BuiltinType{ .kind = BuiltinTypeKind::U32 });
    else if (match(IrTokenKind::KywU8))
      type = m_types_.emplace(BuiltinType{ .kind = BuiltinTypeKind::U8 });

    if (type == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(start.range),
          .message = "Expected a type, like 'int' or 'char'",
          .notes = {},
      });

      while (match(IrTokenKind::Star));
      return nullptr;
    }

    while (match(IrTokenKind::Star)) type = m_types_.emplace(PointerType{ .pointee = type });
    return type;
  }

  [[nodiscard]] auto comparision_kind(IrTokenKind kind) -> IrComparisionOpKind {
    return kind == IrTokenKind::KywEq   ? IrComparisionOpKind::Eq
           : kind == IrTokenKind::KywNe ? IrComparisionOpKind::Ne
           : kind == IrTokenKind::KywLt ? IrComparisionOpKind::Lt
           : kind == IrTokenKind::KywLe ? IrComparisionOpKind::Le
           : kind == IrTokenKind::KywGt ? IrComparisionOpKind::Gt
           : kind == IrTokenKind::KywGe ? IrComparisionOpKind::Ge
                                        : IrComparisionOpKind::Undefined;
  }

  [[nodiscard]] auto build_inst() -> IrInst* {
    const IrToken& start{ current() };

    if (match(IrTokenKind::KywStore)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest = build_slot();
      return m_insts_.emplace(StoreInst{ .arg0 = arg0, .dest = dest }, start.range);
    }

    if (match(IrTokenKind::KywAdd) || match(IrTokenKind::KywSub) || match(IrTokenKind::KywMul) ||
        match(IrTokenKind::KywDiv)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      InstArg arg1;
      bool    idk_unused = build_inst_arg(&arg1);
      _ = idk_unused;

      expect(IrTokenKind::Arrow);

      IrSlot             dest = build_slot();
      IrArithmeticOpKind kind = start.kind == IrTokenKind::KywAdd   ? IrArithmeticOpKind::Add
                                : start.kind == IrTokenKind::KywSub ? IrArithmeticOpKind::Sub
                                : start.kind == IrTokenKind::KywMul ? IrArithmeticOpKind::Mul
                                                                    : IrArithmeticOpKind::Div;
      return m_insts_.emplace(
          ArithmeticInst{
              .arg0 = arg0,
              .arg1 = arg1,
              .dest = dest,
              .kind = kind,
          },
          start.range);
    }

    if (match(IrTokenKind::KywEq) || match(IrTokenKind::KywNe) || match(IrTokenKind::KywLt) ||
        match(IrTokenKind::KywLe) || match(IrTokenKind::KywGt) || match(IrTokenKind::KywGe)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      InstArg arg1;
      bool    idk_unused = build_inst_arg(&arg1);
      _ = idk_unused;

      expect(IrTokenKind::Arrow);

      IrSlot dest = build_slot();
      return m_insts_.emplace(
          ComparisionInst{
              .arg0 = arg0,
              .arg1 = arg1,
              .dest = dest,
              .kind = comparision_kind(start.kind),
          },
          start.range);
    }

    if (match(IrTokenKind::KywJmp)) {
      const IrToken& dest{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(
          JumpInst{
              .dest = dest.text,
              .destRange = dest.range,
          },
          start.range);
    }

    if (match(IrTokenKind::KywCall)) {
      const IrToken& dest{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(
          CallInst{
              .dest = dest.text,
              .destRange = dest.range,
          },
          start.range);
    }

    if (match(IrTokenKind::KywJmpIf) || match(IrTokenKind::KywCallIf)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      IrComparisionOpKind arg1 = comparision_kind(advance().kind);

      InstArg arg2;
      bool    idk_unused = build_inst_arg(&arg2);
      _ = idk_unused;

      expect(IrTokenKind::Arrow);

      const IrToken& dest{ current() };
      expect(IrTokenKind::Identifier);

      return m_insts_.emplace(
          BranchIfInst{
              .arg0 = arg0,
              .arg2 = arg2,
              .dest = dest.text,
              .destRange = dest.range,
              .kind = start.kind == IrTokenKind::KywJmpIf ? BranchIfInstKind::Jump
                                                          : BranchIfInstKind::Call,
              .arg1 = arg1,
          },
          start.range);
    }

    if (match(IrTokenKind::KywAlloc)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest = build_slot();
      return m_insts_.emplace(AllocInst{ .dest = dest, .arg0 = arg0 }, start.range);
    }

    if (match(IrTokenKind::KywFree)) {
      IrSlot arg0 = build_slot();
      return m_insts_.emplace(FreeInst{ .arg0 = arg0 }, start.range);
    }

    if (match(IrTokenKind::KywLoadAddr)) {
      IrSlot arg0 = build_slot();
      expect(IrTokenKind::Arrow);
      IrSlot dest = build_slot();
      return m_insts_.emplace(LoadAddrInst{ .dest = dest, .arg0 = arg0 }, start.range);
    }

    if (match(IrTokenKind::KywStoreAt)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);

      expect(IrTokenKind::Arrow);

      IrSlot dest = build_slot();

      InstArg offset;
      bool    well_obv_unused = build_inst_arg(&offset);
      _ = well_obv_unused;

      return m_insts_.emplace(StoreAtInst{ .arg0 = arg0, .dest1 = offset, .dest = dest },
                              start.range);
    }

    if (match(IrTokenKind::KywStoreAddr)) {
      IrSlot arg0 = build_slot();
      expect(IrTokenKind::Arrow);
      IrSlot dest = build_slot();
      return m_insts_.emplace(StoreAddrInst{ .arg0 = arg0, .dest = dest }, start.range);
    }

    if (match(IrTokenKind::KywRet)) {
      std::vector<InstArg> args;

      if (!match(IrTokenKind::KywVoid)) {
        do {
          InstArg arg;
          bool    ok = build_inst_arg(&arg);
          if (ok)
            args.push_back(arg);
        } while (match(IrTokenKind::Comma) && !peek(IrTokenKind::EndOfFile));

        if (args.empty()) {
          Diagnostics::report(Diagnostic{
              .severity = DiagnosticSeverity::Error,
              .range = static_cast<SourceMultiRange>(start.range),
              .message =
                  "Expected 'ret void' for returning with zero arguments (returning nothing)",
              .notes = {},
          });
        }
      }

      return m_insts_.emplace(RetInst{ .args = std::move(args) }, start.range);
    }

    if (match(IrTokenKind::KywExit)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);
      return m_insts_.emplace(ExitInst{ .arg0 = arg0 }, start.range);
    }

    if (match(IrTokenKind::KywPrint)) {
      InstArg arg0;
      bool    _ = build_inst_arg(&arg0);
      return m_insts_.emplace(PrintInst{ .arg0 = arg0 }, start.range);
    }

    if (match(IrTokenKind::Identifier)) {
      expect(IrTokenKind::Colon);
      const IrToken& type_start{ current() };
      Type*          type = build_type();
      return m_insts_.emplace(
          DeclareInst{
              .ident = start.text,
              .typeRange = source_multi_range_from(type_start.range, current().range),
              .type = type,
          },
          start.range);
    }

    Diagnostics::report(Diagnostic{
        .severity = DiagnosticSeverity::Error,
        .range = static_cast<SourceMultiRange>(start.range),
        .message =
            std::format("Expected an instruction, found '{}'", format_ir_token_kind(start.kind)),
        .notes = {},
    });

    advance_cur();
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

    bool invalid_end{ insts.empty() };

    if (!invalid_end) {
      const IrInstData& back_inst_data{ insts.back()->data };
      bool              is_ret{ std::holds_alternative<RetInst>(back_inst_data) };
      bool              is_jmp{ std::holds_alternative<JumpInst>(back_inst_data) };
      bool              is_exit{ std::holds_alternative<ExitInst>(back_inst_data) };
      invalid_end = !is_jmp && !is_ret && !is_exit;
    }

    if (invalid_end) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range =
              static_cast<SourceMultiRange>(insts.empty() ? ident.range : insts.back()->instRange),
          .message = "Expected a 'ret', 'jmp' or 'exit' instruction at the end of a label",
          .notes = {},
      });
    }

    return m_branches_.emplace(insts, ident.text, start.range);
  }

  [[nodiscard]] auto build_function() -> IrFunction* {
    const IrToken& ident{ current() };

    if (!expect(IrTokenKind::Identifier)) {
      // TODO(jld-wk): recovery stuff!!
    }
    if (!expect(IrTokenKind::OpenParen)) {
      // same thing... again..
    }

    std::vector<IrFunctionParam> params;
    if (peek(IrTokenKind::Identifier)) {
      do {
        const IrToken& ident{ current() };
        expect(IrTokenKind::Identifier);
        expect(IrTokenKind::Colon);
        const IrToken& type_start{ current() };
        Type*          type = build_type();
        params.emplace_back(ident.text, source_multi_range_from(type_start.range, current().range),
                            ident.range, type);
      } while (match(IrTokenKind::Comma) && !peek(IrTokenKind::EndOfFile));
    } else if (!match(IrTokenKind::KywVoid)) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(ident.range),
          .message =
              std::format("Expected '{} (void)' for a function with zero parameters", ident.text),
          .notes = {},
      });
    }

    if (!expect(IrTokenKind::CloseParen)) {
      // same thing... again..
    }

    if (!expect(IrTokenKind::Colon)) {
      // same thing... again..
    }

    std::vector<IrInst*> insts;
    while (!peek(IrTokenKind::Dot) && !peek(IrTokenKind::KywEnd) && !peek(IrTokenKind::EndOfFile)) {
      IrInst* inst = build_inst();
      insts.push_back(inst);
    }

    bool invalid_end{ insts.empty() };

    if (!invalid_end) {
      const IrInstData& back_inst_data{ insts.back()->data };
      bool              is_ret{ std::holds_alternative<RetInst>(back_inst_data) };
      bool              is_jmp{ std::holds_alternative<JumpInst>(back_inst_data) };
      bool              is_exit{ std::holds_alternative<ExitInst>(back_inst_data) };
      invalid_end = !is_jmp && !is_ret && !is_exit;
    }

    if (invalid_end) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range =
              static_cast<SourceMultiRange>(insts.empty() ? ident.range : insts.back()->instRange),
          .message = "Expected a 'ret', 'jmp' or 'exit' instruction at the end of the instructions "
                     "of a function",
          .notes = {},
      });
    }

    std::unordered_map<std::string_view, IrLabel*> labels;

    while (!peek(IrTokenKind::KywEnd) && !peek(IrTokenKind::EndOfFile)) {
      IrLabel* label = build_label();
      labels[label->ident] = label;
    }

    expect(IrTokenKind::KywEnd);
    return m_functions_.emplace(labels, insts, params, ident.text, ident.range);
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
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(current().range),
          .message = std::format("Expected {}, found {}", format_ir_token_kind(kind),
                                 format_ir_token_kind(current().kind)),
          .notes = {},
      });
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