// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INTERPRETER_H
#define JLD_MCC_IR_INTERPRETER_H

#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "arena.h"
#include "diagnostic/core.h"
#include "diagnostic/source.h"
#include "diagnostic/source_manager.h"
#include "ir/insts.h"
#include "ir_syntaxer.h"
#include "ir_tokenizer.h"
#include "utility.h"

class IrScope {
 public:
  void store(std::string_view identifier, size_t value, std::string_view cur_branch = "") {
    if (m_funcVariables_.contains(identifier)) {
      m_funcVariables_[identifier] = value;
      return;
    }

    if (!cur_branch.empty()) {
      auto branch = m_branchVariables_[cur_branch];
      branch[identifier] = value;
      return;
    }

    m_funcVariables_[identifier] = value;
  }

  auto query(std::string_view identifier, std::string_view cur_branch = "") -> size_t {
    if (!cur_branch.empty()) {
      const auto branch = m_branchVariables_[cur_branch];
      const auto it = branch.find(identifier);
      if (it != branch.end())
        return it->second;
    }

    const auto it = m_funcVariables_.find(identifier);
    if (it != m_funcVariables_.end())
      return it->second;

    // TODO(jld-wk): usage of undefined variable
    return 0;
  }

  auto query_ptr(std::string_view identifier, std::string_view cur_branch = "") -> size_t* {
    if (!cur_branch.empty()) {
      auto branch = m_branchVariables_[cur_branch];
      auto it = branch.find(identifier);
      if (it != branch.end())
        return &it->second;
    }

    auto it = m_funcVariables_.find(identifier);
    if (it != m_funcVariables_.end())
      return &it->second;

    // TODO(jld-wk): usage of undefined variable
    return nullptr;
  }

  void clear() {
    m_funcVariables_.clear();
    m_branchVariables_.clear();
  }

 private:
  std::unordered_map<std::string_view, size_t> m_funcVariables_;
  std::unordered_map<std::string_view, std::unordered_map<std::string_view, size_t>>
      m_branchVariables_;
};

class IrInterpreter {
 public:
  void interpret_file(const std::string& filepath, SourceManager& manager) {
    FileId           file = manager.open(filepath);
    std::span<char>  source_buf = manager.query(file).source;
    std::string_view source{ source_buf.data(), source_buf.size() };

    IrTokenizer          tokenizer;
    std::vector<IrToken> tokens = tokenizer.tokenize(file, source);

    for (const IrToken& token : tokens)
      std::println("{} -> {}", format_ir_token_kind(token.kind), token.text);
    std::println("");

    IrSyntaxer               syntaxer{ tokens };
    std::vector<IrFunction*> functions = syntaxer.build();

    IrFunction* entry_function = nullptr;

    for (IrFunction* function : functions) {
      m_funcScopes_[function] = m_scopes_.emplace();
      m_functions_[function->identifier] = function;

      if (function->identifier == "entry")
        entry_function = function;
    }

    if (entry_function == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = SourceRange{},
          .message = "No entry function found! You have to define an entry point.",
          .notes = {},
      });
    } else {
      execute_function(entry_function);
    }

    if (m_exitCode_.has_value())
      std::println("[Interpreter] Program exited with code {}", m_exitCode_.value());
    else {
      Diagnostics::print();
      std::println("[Interpreter] Program didn't exit correctly");
    }
  }

 private:
  void store_slot(const IrSlot& slot, size_t val) {
    switch (slot.kind) {
      case IrSlotKind::Local:
        m_curScope_->store(slot.identifier, val, m_curBranch_);
        return;
      case IrSlotKind::Global:
      case IrSlotKind::Register:
      case IrSlotKind::Return:
        break;
      case IrSlotKind::Argument:
        size_t slot_idx{ 0 };
        std::from_chars(slot.identifier.data(), slot.identifier.data() + slot.identifier.length(),
                        slot_idx);
        m_argSlots_[slot_idx] = val;
        return;
    }

    assert(false);
  }

  auto query_slot(const IrSlot& slot) -> size_t {
    switch (slot.kind) {
      case IrSlotKind::Local:
        return m_curScope_->query(slot.identifier, m_curBranch_);
      case IrSlotKind::Global:
      case IrSlotKind::Register:
      case IrSlotKind::Argument:
        break;
      case IrSlotKind::Return:
        size_t slot_idx{ 0 };
        std::from_chars(slot.identifier.data(), slot.identifier.data() + slot.identifier.length(),
                        slot_idx);
        return m_argSlots_[slot_idx];
    }

    assert(false);
  }

  auto interpret_param(const InstParameter& param) -> size_t {
    return std::visit(Overload{
                          [](size_t constant) -> size_t { return constant; },
                          [&](const IrSlot& slot) -> size_t {
                            switch (slot.kind) {
                              case IrSlotKind::Local:
                                return m_curScope_->query(slot.identifier, m_curBranch_);
                              case IrSlotKind::Global:
                              case IrSlotKind::Register:
                              case IrSlotKind::Argument:
                                break;
                              case IrSlotKind::Return:
                                size_t slot_idx{ 0 };
                                std::from_chars(slot.identifier.data(),
                                                slot.identifier.data() + slot.identifier.length(),
                                                slot_idx);
                                return m_retSlots_[slot_idx];
                            }

                            assert(false);
                            return 0;
                          },
                      },
                      param);
  }

  auto comparision_result(size_t a, size_t b, ComparisionInstKind kind) -> bool {
    switch (kind) {
      case ComparisionInstKind::Eq:
        return a == b;
      case ComparisionInstKind::Ne:
        return a != b;
      case ComparisionInstKind::Lt:
        return a < b;
      case ComparisionInstKind::Le:
        return a <= b;
      case ComparisionInstKind::Gt:
        return a > b;
      case ComparisionInstKind::Ge:
        return a >= b;
    }
    return false;
  }

  auto interpret_inst(IrInst* p_inst) -> bool {
    return std::visit(Overload{
                          [&](const StoreInst& inst) -> bool {
                            store_slot(inst.slot, interpret_param(inst.p1));
                            return true;
                          },
                          [&](const ArithmeticInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };
                            size_t b{ interpret_param(inst.p2) };

                            switch (inst.kind) {
                              case ArithmeticInstKind::Add:
                                store_slot(inst.slot, a + b);
                                return true;
                              case ArithmeticInstKind::Sub:
                                store_slot(inst.slot, a - b);
                                return true;
                              case ArithmeticInstKind::Mul:
                                store_slot(inst.slot, a * b);
                                return true;
                              case ArithmeticInstKind::Div:
                                store_slot(inst.slot, a / b);
                                return true;
                            }

                            assert(false);
                            return false;
                          },
                          [&](const ComparisionInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };
                            size_t b{ interpret_param(inst.p2) };
                            store_slot(inst.slot, comparision_result(a, b, inst.kind));

                            return true;
                          },
                          [&](const BranchInst& inst) -> bool {
                            if (inst.kind == BranchInstKind::Jmp) {
                              execute_branch(m_branches_->at(inst.branch));
                              return true;
                            }

                            execute_function(m_functions_[inst.branch]);
                            return true;
                          },
                          [&](const BranchIfInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };
                            size_t b{ interpret_param(inst.p2) };
                            if (!comparision_result(a, b, inst.p3))
                              return true;

                            if (inst.kind == BranchInstKind::Jmp) {
                              execute_branch(m_branches_->at(inst.branch));
                              return true;
                            }

                            execute_function(m_functions_[inst.branch]);
                            return true;
                          },
                          [&](const AllocInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };
                            auto*  addr{ static_cast<uint8_t*>(malloc(a)) };
                            store_slot(inst.slot, reinterpret_cast<size_t>(addr));
                            return true;
                          },
                          [&](const FreeInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };
                            // NOLINTNEXTLINE
                            free(reinterpret_cast<uint8_t*>(a));
                            return true;
                          },
                          [&](const StorePtrInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };
                            size_t b{ interpret_param(inst.p2) };
                            // NOLINTNEXTLINE
                            uint8_t* ptr{ reinterpret_cast<uint8_t*>(query_slot(inst.slot)) };
                            // NOLINTNEXTLINE
                            *(ptr + b) = static_cast<uint8_t>(a);
                            return true;
                          },
                          [&](const StoreAddrInst& inst) -> bool {
                            store_slot(inst.slot, reinterpret_cast<size_t>(
                                                      m_curScope_->query_ptr(inst.p1.identifier)));
                            return true;
                          },
                          [&](const LoadAddrInst& inst) -> bool {
                            size_t p1{ query_slot(inst.p1) };
                            // NOLINTNEXTLINE
                            size_t val{ static_cast<size_t>(*reinterpret_cast<uint8_t*>(p1)) };
                            store_slot(inst.slot, val);
                            return true;
                          },
                          [&](const RetInst& inst) -> bool {
                            m_retSlots_.clear();
                            for (size_t i = 0; i < inst.params.size(); ++i)
                              m_retSlots_[i] = interpret_param(inst.params[i]);
                            return false;
                          },
                          [&](const ExitInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };
                            m_exitCode_ = std::optional{ a };
                            m_exitCalled_ = true;
                            return false;
                          },
                          [&](const DumpInst& inst) -> bool {
                            size_t a{ interpret_param(inst.p1) };

                            switch (inst.kind) {
                              case DumpInstKind::Decimal:
                                std::print("{:d}", a);
                                return true;
                              case DumpInstKind::Char:
                                std::print("{:c}", a);
                                return true;
                            }

                            return false;
                          },
                      },
                      p_inst->variant);
  }

  void execute_branch(IrBranch* branch) {
    m_curBranch_ = branch->identifier;
    for (IrInst* inst : branch->insts) {
      if (!interpret_inst(inst) || m_exitCalled_)
        break;
    }
    m_curBranch_ = "";
  }

  void execute_function(IrFunction* function) {
    m_callStack_.push(function);
    m_branches_ = &function->branches;
    m_curScope_ = m_funcScopes_[function];

    if (m_argSlots_.size() != function->params.size()) {
      assert(false);
    }
    for (size_t i = 0; i < m_argSlots_.size(); ++i)
      m_curScope_->store(function->params[i], m_argSlots_[i]);
    m_argSlots_.clear();

    if (function->insts.empty()) {
      if (function->branches.empty())
        return;

      execute_branch(function->branches.begin()->second);
    } else {
      for (IrInst* inst : function->insts) {
        if (!interpret_inst(inst) || m_exitCalled_)
          break;
      }
    }

    m_callStack_.pop();
    if (!m_callStack_.empty()) {
      IrFunction* top = m_callStack_.top();
      m_branches_ = &top->branches;
      m_curScope_ = m_funcScopes_[top];
    }
  }

 private:
  std::optional<size_t> m_exitCode_{ std::nullopt };
  bool                  m_exitCalled_{ false };

  std::unordered_map<size_t, size_t> m_argSlots_;
  std::unordered_map<size_t, size_t> m_retSlots_;

  Arena<IrScope>                            m_scopes_;
  IrScope*                                  m_curScope_;
  std::unordered_map<IrFunction*, IrScope*> m_funcScopes_;

  std::stack<IrFunction*> m_callStack_;

  std::string_view m_curBranch_;

  std::unordered_map<std::string_view, IrBranch*>*  m_branches_;
  std::unordered_map<std::string_view, IrFunction*> m_functions_;
};

#endif  // JLD_MCC_IR_INTERPRETER_H