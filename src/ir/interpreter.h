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
#include "ir/values.h"
#include "ir_syntaxer.h"
#include "ir_tokenizer.h"
#include "type_arena.h"
#include "types.h"
#include "utility.h"

/*

TODOs:

- Variables have to be declared before they can be stored
- Store must do type checks (any instruction must)
- Add a binary overload handler
- Add a cast keyword (e.g. char to int) and a function to do implicit conversions and then return,
but add safety!
- Add a size_t/uintptr_t type
- Make alloc type safe with something like: alloc 4 -> int* [ptr]
- Check free pointers (double-free's/invalid ptrs)
- Check for undefined ir values everywhere
- Use Undeclared as ir value for variable declaration
- Make exit inst expected a uint8?
- Handle every assert
- Store at should offset using the offset and type (1 -> 1 * sizeof(type))

*/

class IrScope {
 public:
  void store(std::string_view identifier, IrValue value, std::string_view cur_branch = "") {
    if (m_irFuncLocalVars_.contains(identifier)) {
      m_irFuncLocalVars_[identifier] = value;
      return;
    }

    if (!cur_branch.empty()) {
      auto& branch = m_irBranchLocalVars_[cur_branch];
      branch[identifier] = value;
      return;
    }

    m_irFuncLocalVars_[identifier] = value;
  }

  auto query(std::string_view identifier, std::string_view cur_branch = "") -> IrValue {
    if (!cur_branch.empty()) {
      const auto& branch = m_irBranchLocalVars_[cur_branch];
      const auto& it = branch.find(identifier);
      if (it != branch.end())
        return it->second;
    }

    const auto& it = m_irFuncLocalVars_.find(identifier);
    if (it != m_irFuncLocalVars_.end())
      return it->second;

    // TODO(jld-wk): usage of undefined variable
    return IrValue{ .variant = IrValueUndefined{}, .type = nullptr };
  }

  auto query_as_ptr(std::string_view identifier, std::string_view cur_branch = "") -> IrValue* {
    if (!cur_branch.empty()) {
      auto& branch = m_irBranchLocalVars_[cur_branch];
      auto  it = branch.find(identifier);
      if (it != branch.end())
        return &it->second;
    }

    auto it = m_irFuncLocalVars_.find(identifier);
    if (it != m_irFuncLocalVars_.end())
      return &it->second;

    // TODO(jld-wk): usage of undefined variable
    return nullptr;
  }

  void clear() {
    m_irFuncLocalVars_.clear();
    m_irBranchLocalVars_.clear();
  }

 private:
  std::unordered_map<std::string_view, IrValue> m_irFuncLocalVars_;
  std::unordered_map<std::string_view, std::unordered_map<std::string_view, IrValue>>
      m_irBranchLocalVars_;
};

class IrInterpreter {
 public:
  explicit IrInterpreter(TypeArena& types)
      : m_types_{ types } {}

  void interpret_file(const std::string& filepath, SourceManager& manager) {
    FileId           file = manager.open(filepath);
    std::span<char>  source_buf = manager.query(file).source;
    std::string_view source{ source_buf.data(), source_buf.size() };

    IrTokenizer          tokenizer;
    std::vector<IrToken> tokens = tokenizer.tokenize(file, source);

    for (const IrToken& token : tokens)
      std::println("{} -> {}", format_ir_token_kind(token.kind), token.text);
    std::println("");

    IrSyntaxer               syntaxer{ tokens, m_types_ };
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
  void store_slot(const IrSlot& slot, IrValue val) {
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

  auto query_slot(const IrSlot& slot) -> IrValue {
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

  auto interpret_param(const InstArg& param) -> IrValue {
    return std::visit(Overload{
                          [](const IrValue& constant) -> IrValue { return constant; },
                          [&](const IrSlot& slot) -> IrValue {
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
                            return IrValue{ .variant = IrValueUndefined{}, .type = nullptr };
                          },
                      },
                      param);
  }

  auto comparision_result(int a, int b, ComparisionInstKind kind) -> bool {
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
      case ComparisionInstKind::Undefined:
        assert(false);
        break;
    }

    return false;
  }

  auto interpret_inst(IrInst* p_inst) -> bool {
    return std::visit(
        Overload{
            [&](const DeclareInst& inst) -> bool {
              m_curScope_->store(inst.identifier,
                                 IrValue{ .variant = IrValueUndefined{}, .type = inst.type });
              return true;
            },
            [&](const StoreInst& inst) -> bool {
              store_slot(inst.dest, interpret_param(inst.arg0));
              return true;
            },
            [&](const ArithmeticInst& inst) -> bool {
              IrValue arg0{ interpret_param(inst.arg0) };
              IrValue arg1{ interpret_param(inst.arg1) };

              // TODO(jld-wk): create a binary overload handler

              return std::visit(Overload{ [&](IrValuePointer& value) -> bool {
                                           int b = std::get_if<IrValueInt>(&arg1.variant)->value;

                                           switch (inst.kind) {
                                             case ArithmeticInstKind::Add:
                                               value.addr += static_cast<std::uintptr_t>(b);
                                               store_slot(inst.dest, arg0);
                                               return true;
                                             case ArithmeticInstKind::Sub:
                                               value.addr -= static_cast<std::uintptr_t>(b);
                                               store_slot(inst.dest, arg0);
                                               return true;
                                             case ArithmeticInstKind::Mul:
                                             case ArithmeticInstKind::Div:
                                               assert(false);
                                               return false;
                                           }

                                           assert(false);
                                           return false;
                                         },
                                          [&](IrValueInt& value) -> bool {
                                            int b = std::get_if<IrValueInt>(&arg1.variant)->value;

                                            switch (inst.kind) {
                                              case ArithmeticInstKind::Add:
                                                value.value += b;
                                                store_slot(inst.dest, arg0);
                                                return true;
                                              case ArithmeticInstKind::Sub:
                                                value.value -= b;
                                                store_slot(inst.dest, arg0);
                                                return true;
                                              case ArithmeticInstKind::Mul:
                                                value.value *= b;
                                                store_slot(inst.dest, arg0);
                                                return true;
                                              case ArithmeticInstKind::Div:
                                                value.value /= b;
                                                store_slot(inst.dest, arg0);
                                                return true;
                                            }

                                            assert(false);
                                            return false;
                                          },
                                          [&](IrValueChar&) -> bool {
                                            assert(false);
                                            return false;
                                          },
                                          [&](IrValueUndefined&) -> bool {
                                            assert(false);
                                            return false;
                                          } },
                                arg0.variant);

              assert(false);
              return false;
            },
            [&](const ComparisionInst& inst) -> bool {
              IrValue arg0{ interpret_param(inst.arg0) };
              IrValue arg1{ interpret_param(inst.arg1) };

              // TODO(jld-wk): create a binary overload handler

              return std::visit(Overload{ [&](IrValuePointer&) -> bool {
                                           assert(false);
                                           return false;
                                         },
                                          [&](IrValueInt& value) -> bool {
                                            int b = std::get_if<IrValueInt>(&arg1.variant)->value;
                                            value.value =
                                                comparision_result(value.value, b, inst.kind);
                                            store_slot(inst.dest, arg0);
                                            return true;
                                          },
                                          [&](IrValueChar&) -> bool {
                                            assert(false);
                                            return false;
                                          },
                                          [&](IrValueUndefined&) -> bool {
                                            assert(false);
                                            return false;
                                          } },
                                arg0.variant);

              assert(false);
              return true;
            },
            [&](const BranchInst& inst) -> bool {
              if (inst.kind == BranchInstKind::Jmp) {
                execute_branch(m_labels_->at(inst.dest));
                return true;
              }

              execute_function(m_functions_[inst.dest]);
              return true;
            },
            [&](const BranchIfInst& inst) -> bool {
              IrValue arg0{ interpret_param(inst.arg0) };
              IrValue arg2{ interpret_param(inst.arg2) };

              int a{ 0 };

              auto* i_irv_ptr = std::get_if<IrValueInt>(&arg0.variant);
              if (i_irv_ptr == nullptr)
                a = static_cast<unsigned char>(std::get_if<IrValueChar>(&arg0.variant)->value);
              else
                a = i_irv_ptr->value;

              int b = std::get_if<IrValueInt>(&arg2.variant)->value;
              if (!comparision_result(a, b, inst.arg1))
                return true;

              if (inst.kind == BranchInstKind::Jmp) {
                execute_branch(m_labels_->at(inst.dest));
                return true;
              }

              execute_function(m_functions_[inst.dest]);
              return true;
            },
            [&](const AllocInst& inst) -> bool {
              IrValue arg0{ interpret_param(inst.arg0) };
              int     a = std::get_if<IrValueInt>(&arg0.variant)->value;
              auto*   addr{ static_cast<char*>(malloc(static_cast<size_t>(a))) };
              store_slot(
                  inst.dest,
                  IrValue{
                      .variant = IrValuePointer{ .addr = reinterpret_cast<std::uintptr_t>(addr) },
                      .type =
                          m_types_.emplace(PointerType{ .pointee = m_types_.emplace(BuiltinType{
                                                            .kind = BuiltinTypeKind::Char }) }) });
              return true;
            },
            [&](const FreeInst& inst) -> bool {
              IrValue        arg0{ query_slot(inst.arg0) };
              std::uintptr_t a = std::get_if<IrValuePointer>(&arg0.variant)->addr;
              // NOLINTNEXTLINE
              free(reinterpret_cast<char*>(a));
              return true;
            },
            [&](const StoreAtInst& inst) -> bool {
              IrValue arg0{ interpret_param(inst.arg0) };
              IrValue dest{ interpret_param(inst.dest) };
              IrValue offset{ interpret_param(inst.offset) };

              int a{ 0 };

              auto* i_irv_ptr = std::get_if<IrValueInt>(&arg0.variant);
              if (i_irv_ptr == nullptr)
                a = static_cast<unsigned char>(std::get_if<IrValueChar>(&arg0.variant)->value);
              else
                a = i_irv_ptr->value;

              int            off = std::get_if<IrValueInt>(&offset.variant)->value;
              std::uintptr_t dest_ir_val = std::get_if<IrValuePointer>(&dest.variant)->addr;
              // NOLINTNEXTLINE
              char* ptr{ reinterpret_cast<char*>(dest_ir_val) };
              // NOLINTNEXTLINE
              *(ptr + off) = static_cast<char>(a);
              return true;
            },
            [&](const StoreAddrInst& inst) -> bool {
              IrValue* arg0 = m_curScope_->query_as_ptr(inst.arg0.identifier, m_curBranch_);
              store_slot(inst.dest, IrValue{
                                        .variant =
                                            IrValuePointer{
                                                .addr = reinterpret_cast<std::uintptr_t>(arg0),
                                            },
                                        .type = m_types_.emplace(PointerType{
                                            .pointee = arg0->type,
                                        }),
                                    });
              return true;
            },
            [&](const LoadAddrInst& inst) -> bool {
              IrValue        arg0{ query_slot(inst.arg0) };
              std::uintptr_t a = std::get_if<IrValuePointer>(&arg0.variant)->addr;

              auto* ptr = std::get_if<PointerType>(&arg0.type->variant);

              return std::visit(
                  Overload{
                      [&](BuiltinType type) -> bool {
                        switch (type.kind) {
                          case BuiltinTypeKind::Char:
                            store_slot(inst.dest,
                                       IrValue{ .variant =
                                                    IrValueChar{
                                                        // NOLINTNEXTLINE
                                                        .value = *reinterpret_cast<char*>(a),
                                                    },
                                                .type = ptr->pointee });
                            return true;
                          case BuiltinTypeKind::Int:
                            store_slot(inst.dest,
                                       IrValue{ .variant =
                                                    // NOLINTNEXTLINE
                                                IrValueInt{ .value = *reinterpret_cast<int*>(a) },
                                                .type = ptr->pointee });
                            return true;
                        }

                        assert(false);
                        return false;
                      },
                      [&](PointerType) -> bool {
                        store_slot(inst.dest,
                                   IrValue{ .variant =
                                                IrValuePointer{
                                                    // NOLINTNEXTLINE
                                                    .addr = *reinterpret_cast<std::uintptr_t*>(a) },
                                            .type = ptr->pointee });
                        return true;
                      } },
                  ptr->pointee->variant);
            },
            [&](const RetInst& inst) -> bool {
              m_retSlots_.clear();
              for (size_t i = 0; i < inst.values.size(); ++i)
                m_retSlots_[i] = interpret_param(inst.values[i]);
              return false;
            },
            [&](const ExitInst& inst) -> bool {
              IrValue arg0{ interpret_param(inst.arg0) };
              int     a = std::get_if<IrValueInt>(&arg0.variant)->value;
              m_exitCode_ = std::optional{ a };
              m_exitCalled_ = true;
              return false;
            },
            [&](const DumpInst& inst) -> bool {
              IrValue arg0{ interpret_param(inst.arg0) };

              return std::visit(Overload{ [](IrValueInt value) -> bool {
                                           std::print("{:d}", value.value);
                                           return true;
                                         },
                                          [](IrValueChar value) -> bool {
                                            std::print("{:c}", value.value);
                                            return true;
                                          },
                                          [](IrValuePointer value) -> bool {
                                            std::print("{}", value.addr);
                                            return true;
                                          },
                                          [](IrValueUndefined) -> bool {
                                            std::print("Undefined IR value");
                                            return true;
                                          } },
                                arg0.variant);
            },
        },
        p_inst->variant);
  }

  void execute_branch(IrLabel* branch) {
    m_curBranch_ = branch->identifier;
    for (IrInst* inst : branch->insts) {
      if (!interpret_inst(inst) || m_exitCalled_)
        break;
    }
    m_curBranch_ = "";
  }

  void execute_function(IrFunction* function) {
    m_callStack_.push(function);
    m_labels_ = &function->labels;
    m_curScope_ = m_funcScopes_[function];

    if (m_argSlots_.size() != function->params.size()) {
      assert(false);
    }
    for (size_t i = 0; i < m_argSlots_.size(); ++i) {
      assert(function->params[i].type == m_argSlots_[i].type);
      m_curScope_->store(function->params[i].identifier, m_argSlots_[i]);
    }
    m_argSlots_.clear();

    if (function->insts.empty()) {
      if (function->labels.empty())
        return;

      execute_branch(function->labels.begin()->second);
    } else {
      for (IrInst* inst : function->insts) {
        if (!interpret_inst(inst) || m_exitCalled_)
          break;
      }
    }

    m_callStack_.pop();
    if (!m_callStack_.empty()) {
      IrFunction* top = m_callStack_.top();
      m_labels_ = &top->labels;
      m_curScope_ = m_funcScopes_[top];
    }
  }

 private:
  TypeArena& m_types_;

  std::optional<int32_t> m_exitCode_{ std::nullopt };
  bool                   m_exitCalled_{ false };

  std::unordered_map<size_t, IrValue> m_argSlots_;
  std::unordered_map<size_t, IrValue> m_retSlots_;

  Arena<IrScope>                            m_scopes_;
  IrScope*                                  m_curScope_{ nullptr };
  std::unordered_map<IrFunction*, IrScope*> m_funcScopes_;

  std::stack<IrFunction*> m_callStack_;

  std::string_view m_curBranch_;

  std::unordered_map<std::string_view, IrLabel*>*   m_labels_{ nullptr };
  std::unordered_map<std::string_view, IrFunction*> m_functions_;
};

#endif  // JLD_MCC_IR_INTERPRETER_H