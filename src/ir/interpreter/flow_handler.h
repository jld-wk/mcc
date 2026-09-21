// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_FLOW_HANDLER_H
#define JLD_MCC_IR_FLOW_HANDLER_H

#include <cassert>
#include <charconv>
#include <cstdint>
#include <format>
#include <map>
#include <stack>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "allocator.h"
#include "arena.h"
#include "diagnostic/core.h"
#include "diagnostic/source.h"
#include "ir/insts.h"
#include "types.h"
#include "utility.h"
#include "values.h"

using IrScopeVars = std::unordered_map<std::string_view, IrValue*>;

struct IrFunctionScope {
  IrScopeVars funcLocalVars;
  IrScopeVars labelLocalVars;
};

using BranchStackElement = std::variant<IrFunction*, IrLabel*>;

class IrFlowHandler {
 public:
  explicit IrFlowHandler(bool* exit, IrInterpreterAllocator& allocator)
      : m_exit_{ exit }
      , m_allocator_{ allocator } {}

  [[nodiscard]] auto declare_local(std::string_view ident, Type* type, SourceRange ident_range)
      -> InterpreterResult {
    IrFunctionScope* scope = m_curFunc_->scope;

    if (m_isInLabel_) {
      auto& label_local_vars = scope->labelLocalVars;

      if (!label_local_vars.contains(ident)) {
        label_local_vars.try_emplace(
            ident, m_allocator_.alloc_stack(IrUndeclaredValue{}, type, ident_range));
        return InterpreterResult::Success;
      }

      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(ident_range),
          .message =
              Diagnostics::format("/B'{}'/R: local variable already declared inside label /B'{}'/R "
                                  "inside function /B'{}'/R",
                                  ident, m_label_, m_curFunc_->ident),
          .notes = { Diagnostic{
              .severity = DiagnosticSeverity::Note,
              .range = static_cast<SourceMultiRange>(
                  m_allocator_.find_stack_alloc(label_local_vars[ident])->range),
              .message = Diagnostics::format("/B'{}'/R previously declared here", ident),
              .notes = {},
          } },
      });

      *m_exit_ = true;
      return InterpreterResult::Error;
    }

    IrScopeVars& func_local_vars = scope->funcLocalVars;
    if (!func_local_vars.contains(ident)) {
      func_local_vars.try_emplace(ident,
                                  m_allocator_.alloc_stack(IrUndeclaredValue{}, type, ident_range));
      return InterpreterResult::Success;
    }

    Diagnostics::report(Diagnostic{
        .severity = DiagnosticSeverity::Error,
        .range = static_cast<SourceMultiRange>(ident_range),
        .message = Diagnostics::format(
            "/B'{}'/R: local variable already declared inside function /B'{}'/R", ident,
            m_curFunc_->ident),
        .notes = { Diagnostic{
            .severity = DiagnosticSeverity::Note,
            .range = static_cast<SourceMultiRange>(
                m_allocator_.find_stack_alloc(func_local_vars[ident])->range),
            .message = Diagnostics::format("/B'{}'/R previously declared here", ident),
            .notes = {},
        } },
    });

    *m_exit_ = true;
    return InterpreterResult::Error;
  }

  [[nodiscard]] auto store_local(std::string_view ident, const IrValue* val,
                                 SourceRange ident_range) -> InterpreterResult {
    IrFunctionScope* scope = m_curFunc_->scope;

    if (m_isInLabel_) {
      auto& label_local_vars = scope->labelLocalVars;
      if (label_local_vars.contains(ident)) {
        store_impl(label_local_vars[ident], val, ident_range);
        return InterpreterResult::Success;
      }

      IrScopeVars& func_local_vars = scope->funcLocalVars;
      if (func_local_vars.contains(ident)) {
        store_impl(func_local_vars[ident], val, ident_range);
        return InterpreterResult::Success;
      }

      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(ident_range),
          .message = Diagnostics::format("/B'{}'/R: local variable not declared inside label "
                                         "/B'{}'/R inside function /B'{}'/R",
                                         ident, m_label_, m_curFunc_->ident),
          .notes = {},
      });

      *m_exit_ = true;
      return InterpreterResult::Error;
    }

    IrScopeVars& func_local_vars = scope->funcLocalVars;
    if (func_local_vars.contains(ident)) {
      store_impl(func_local_vars[ident], val, ident_range);
      return InterpreterResult::Success;
    }

    Diagnostics::report(Diagnostic{
        .severity = DiagnosticSeverity::Error,
        .range = static_cast<SourceMultiRange>(ident_range),
        .message =
            Diagnostics::format("/B'{}'/R: local variable not declared inside function /B'{}'/R",
                                ident, m_curFunc_->ident),
        .notes = {},
    });

    *m_exit_ = true;
    return InterpreterResult::Error;
  }

  [[nodiscard]] auto store_slot(const IrSlot& slot, const IrValue* val, SourceRange store_range)
      -> InterpreterResult {
    switch (slot.slot.type) {
      case IrSlotType::Local:
        return store_local(slot.slot.ident, val, slot.slot.identRange);
      case IrSlotType::Global:
      case IrSlotType::Register:
      case IrSlotType::Return: {
        Diagnostics::report(Diagnostic{
            .severity = DiagnosticSeverity::Error,
            .range = static_cast<SourceMultiRange>(slot.typeRange),
            .message = Diagnostics::format("cannot store to a slot of type /B'{}'/R (disallowed)",
                                           slot.slot.format_type()),
            .notes = {},
        });

        *m_exit_ = true;
        return InterpreterResult::Error;
      }
      case IrSlotType::Argument: {
        uint32_t slot_idx{ 0 };
        std::from_chars(slot.slot.ident.data(), slot.slot.ident.data() + slot.slot.ident.length(),
                        slot_idx);
        m_argSlots_.try_emplace(slot_idx, val, store_range);
        return InterpreterResult::Success;
      }
      case IrSlotType::Undefined:
        break;
    }

    *m_exit_ = true;
    assert(false);
    return InterpreterResult::Error;
  }

  JLD_MCC_FORCE_INLINE [[nodiscard]] auto store_slot(const IrSlot& slot, const IrValue& val,
                                                     SourceRange store_range) -> InterpreterResult {
    return store_slot(slot, &val, store_range);
  }

  void clear_ret(const SourceMultiRange& ret_range) {
    m_retSlots_.clear();
    m_retSlotsRange_ = ret_range;
    m_retSlotsFunc_ = m_curFunc_->ident;
  }

  void emplace_ret(const IrValue* val, SourceRange store_range) {
    m_retSlots_.emplace_back(*val, store_range);
  }

  [[nodiscard]] auto find_local(std::string_view ident, SourceRange ident_range) -> IrValue* {
    IrFunctionScope* scope = m_curFunc_->scope;

    if (m_isInLabel_) {
      auto& label_local_vars = scope->labelLocalVars;
      if (label_local_vars.contains(ident))
        return label_local_vars[ident];

      IrScopeVars& func_local_vars = scope->funcLocalVars;
      if (func_local_vars.contains(ident))
        return func_local_vars[ident];

      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(ident_range),
          .message = Diagnostics::format("/B'{}'/R: local variable not declared inside label "
                                         "/B'{}'/R inside function /B'{}'/R",
                                         ident, m_label_, m_curFunc_->ident),
          .notes = {},
      });

      *m_exit_ = true;
      return nullptr;
    }

    IrScopeVars& func_local_vars = scope->funcLocalVars;
    if (func_local_vars.contains(ident))
      return func_local_vars[ident];

    Diagnostics::report(Diagnostic{
        .severity = DiagnosticSeverity::Error,
        .range = static_cast<SourceMultiRange>(ident_range),
        .message =
            Diagnostics::format("/B'{}'/R: local variable not declared inside function /B'{}'/R",
                                ident, m_curFunc_->ident),
        .notes = {},
    });

    *m_exit_ = true;
    return nullptr;
  }

  [[nodiscard]] auto find_slot(const IrUntypedSlot& slot, SourceRange type_range) -> IrValue* {
    switch (slot.type) {
      case IrSlotType::Local:
        return find_local(slot.ident, slot.identRange);
      case IrSlotType::Global:
      case IrSlotType::Register:
      case IrSlotType::Argument: {
        Diagnostics::report(Diagnostic{
            .severity = DiagnosticSeverity::Error,
            .range = static_cast<SourceMultiRange>(type_range),
            .message = Diagnostics::format("cannot lookup a slot of type /B'{}'/R (disallowed)",
                                           slot.format_type()),
            .notes = {},
        });

        *m_exit_ = true;
        return nullptr;
      }
      case IrSlotType::Return: {
        uint32_t slot_idx{ 0 };
        std::from_chars(slot.ident.data(), slot.ident.data() + slot.ident.length(), slot_idx);

        if (slot_idx < m_retSlots_.size())
          return &m_retSlots_[slot_idx].val;

        Diagnostics::report(Diagnostic{
            .severity = DiagnosticSeverity::Error,
            .range = static_cast<SourceMultiRange>(slot.identRange),
            .message = Diagnostics::format("no return value has been set for /Bret/R slot /B{}/R",
                                           slot_idx),
            .notes = { Diagnostic{
                .severity = DiagnosticSeverity::Note,
                .range = m_retSlotsRange_,
                .message = Diagnostics::format("return values from function /B'{}'/R set here",
                                               m_retSlotsFunc_),
                .notes = {},
            } },
        });

        *m_exit_ = true;
        return nullptr;
      }
      case IrSlotType::Undefined:
        break;
    }

    *m_exit_ = true;
    assert(false);
    return nullptr;
  }

  void add_function(IrFunction* function) {
    function->scope = m_scopes_.emplace();
    m_functions_[function->ident] = function;
  }

  [[nodiscard]] auto enter_function(IrFunction* function, SourceRange call_range)
      -> InterpreterResult {
    m_curFunc_ = function;
    m_branchStack_.emplace(function);

    if (m_argSlots_.size() != function->params.size()) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(call_range),
          .message = Diagnostics::format(
              "expected an /Bargument count of {}/R but received /B{}/R for function call /B'{}'/R",
              function->params.size(), m_argSlots_.size(), function->ident),
          .notes = { Diagnostic{
              .severity = DiagnosticSeverity::Note,
              .range = static_cast<SourceMultiRange>(function->identRange),
              .message =
                  Diagnostics::format("/B'{}'/R with a /Bparameter count of {}/R declared here",
                                      function->ident, function->params.size()),
              .notes = {},
          } },
      });

      *m_exit_ = true;
      return InterpreterResult::Error;
    }

    for (uint32_t i = 0; i < m_argSlots_.size(); ++i) {
      IrFunctionParam& func_param = function->params[i];
      ArgSlot&         arg_slot = m_argSlots_[i];

      if (func_param.type != arg_slot.val->type) {
        Diagnostics::report(Diagnostic{
            .severity = DiagnosticSeverity::Error,
            .range = func_param.typeRange,
            .message = Diagnostics::format("expected a /Btype of '{}'/R but received /B'{}'/R for "
                                           "/Bparameter {}/R of function /B'{}'/R",
                                           func_param.type->format(), arg_slot.val->type->format(),
                                           i + 1, function->ident),
            .notes = { Diagnostic{
                .severity = DiagnosticSeverity::Note,
                .range = static_cast<SourceMultiRange>(arg_slot.setRange),
                .message = "argument set here",
                .notes = {},
            } },
        });

        *m_exit_ = true;
        return InterpreterResult::Error;
      }

      InterpreterResult _ = declare_local(func_param.ident, func_param.type, func_param.identRange);
      _ = store_local(func_param.ident, arg_slot.val, func_param.identRange);
    }

    m_argSlots_.clear();
    return InterpreterResult::Success;
  }

  auto in_label() const -> bool {
    return m_isInLabel_;
  }

  auto branch_stack() const -> const std::stack<BranchStackElement>& {
    return m_branchStack_;
  }

  void enter_label(IrLabel* label) {
    m_isInLabel_ = true;
    m_label_ = label->ident;
    m_branchStack_.emplace(label);
  }

  [[nodiscard]] auto exit_func_or_label() -> InterpreterResult {
    BranchStackElement self = m_branchStack_.top();
    m_branchStack_.pop();

    if (std::holds_alternative<IrFunction*>(self)) {
      auto& func_local_vars = m_curFunc_->scope->funcLocalVars;
      for (auto& [ident, val] : func_local_vars) {
        if (m_allocator_.free_stack(val, m_allocator_.find_stack_alloc(val)->range) ==
            InterpreterResult::Error)
          return InterpreterResult::Error;
      }
      func_local_vars.clear();
    } else {
      auto& label_local_vars = m_curFunc_->scope->labelLocalVars;
      for (auto& [ident, val] : label_local_vars) {
        if (m_allocator_.free_stack(val, m_allocator_.find_stack_alloc(val)->range) ==
            InterpreterResult::Error)
          return InterpreterResult::Error;
      }
      label_local_vars.clear();
    }

    if (m_branchStack_.empty())
      return InterpreterResult::Success;

    BranchStackElement top = m_branchStack_.top();
    auto**             function = std::get_if<IrFunction*>(&top);
    if (function != nullptr) {
      m_isInLabel_ = false;
      m_curFunc_ = *function;
      return InterpreterResult::Success;
    }

    auto* label = *std::get_if<IrLabel*>(&top);
    if (label != nullptr) {
      m_isInLabel_ = true;
      m_label_ = label->ident;
    }
    return InterpreterResult::Success;
  }

  [[nodiscard]] auto label(std::string_view ident, SourceRange ident_range) -> IrLabel* {
    const auto it = m_curFunc_->labels.find(ident);
    if (it == m_curFunc_->labels.end()) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(ident_range),
          .message = Diagnostics::format("/B'{}'/R: label not declared inside function /B'{}'/R",
                                         ident, m_curFunc_->ident),
          .notes = {},
      });

      *m_exit_ = true;
      return nullptr;
    }

    return it->second;
  }

  [[nodiscard]] auto function(std::string_view ident, SourceRange ident_range) -> IrFunction* {
    const auto it = m_functions_.find(ident);
    if (it == m_functions_.end()) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(ident_range),
          .message = Diagnostics::format("/B'{}'/R: function not declared", ident),
          .notes = {},
      });

      *m_exit_ = true;
      return nullptr;
    }

    return it->second;
  }

 private:
  void store_impl(IrValue* var, const IrValue* val, SourceRange var_range) {
    // TODO(jld-wk): type checks

    auto* var_ptr = std::get_if<IrPointerValue>(&var->data);
    if (var_ptr != nullptr && var_ptr->origin == IrPointerOrigin::Heap) {
      const auto* val_ptr = std::get_if<IrPointerValue>(&val->data);
      assert(val_ptr != nullptr);
      m_allocator_.ref_check(var_ptr, val_ptr, var_range);
    }

    var->data = val->data;
  }

 private:
  bool*                   m_exit_{ nullptr };
  IrInterpreterAllocator& m_allocator_;

  std::string_view m_label_;
  bool             m_isInLabel_{ false };

  std::stack<BranchStackElement> m_branchStack_;
  IrFunction*                    m_curFunc_{ nullptr };

  struct ArgSlot {
    const IrValue* val;
    SourceRange    setRange;
  };

  struct RetSlot {
    IrValue     val;
    SourceRange setRange;
  };

  std::string_view m_retSlotsFunc_;
  SourceMultiRange m_retSlotsRange_;

  std::vector<RetSlot>        m_retSlots_;
  std::map<uint32_t, ArgSlot> m_argSlots_;

  Arena<IrFunctionScope>                            m_scopes_;
  std::unordered_map<std::string_view, IrFunction*> m_functions_;
};

#endif  // JLD_MCC_IR_SCOPE_H