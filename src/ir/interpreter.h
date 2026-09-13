// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INTERPRETER_H
#define JLD_MCC_IR_INTERPRETER_H

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

#include "diagnostic/core.h"
#include "diagnostic/source.h"
#include "diagnostic/source_manager.h"
#include "ir/insts.h"
#include "ir_syntaxer.h"
#include "ir_tokenizer.h"
#include "utility.h"

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

    IrSyntaxer           syntaxer{ tokens };
    std::vector<Branch*> branches = syntaxer.build();

    Branch* entry_branch = nullptr;

    for (Branch* branch : branches) {
      m_branches_[branch->identifier] = branch;
      if (branch->identifier == "entry")
        entry_branch = branch;
    }

    if (entry_branch == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = SourceRange{},
          .message = "No entry branch found! You have to define an entry point.",
          .notes = {},
      });
    } else
      interpret_branch(entry_branch);

    if (m_exitCode_.has_value())
      std::println("[Interpreter] Program exited with code {}", m_exitCode_.value());
    else {
      Diagnostics::print();
      std::println("[Interpreter] Program didn't exit correctly");
    }
  }

 private:
  auto expect_val(const char* name, Inst* inst, bool* con, bool consume = true) -> size_t {
    if (m_stack_.empty()) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = inst->source,
          .message = std::format("'{}' instruction expects 1 value on the stack", name),
          .notes = {},
      });

      m_exit_ = true;
      *con = false;
      return 0;
    }

    size_t a = m_stack_.top();
    if (consume)
      m_stack_.pop();
    return a;
  }

  auto expect_2val(const char* name, Inst* inst, bool* con, size_t* b, bool consume = true)
      -> size_t {
    if (m_stack_.size() < 2) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = inst->source,
          .message = std::format("'{}' instruction expects 2 values on the stack, found {}", name,
                                 m_stack_.size()),
          .notes = {},
      });

      m_exit_ = true;
      *con = false;
      return 0;
    }

    *b = m_stack_.top();
    m_stack_.pop();

    size_t a = m_stack_.top();
    if (consume)
      m_stack_.pop();
    return a;
  }

  auto execute_branch(Inst* inst, std::string_view branch_name, bool con = true) -> bool {
    const auto it{ m_branches_.find(branch_name) };
    if (it == m_branches_.end()) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = inst->source,
          .message = std::format("branch '{}' not found", branch_name),
          .notes = {},
      });

      m_exit_ = true;
      return false;
    }

    interpret_branch(it->second);
    return con;
  }

  enum class OpKind : uint8_t { Add, Sub, Mul, Div, Eq, Ne, Gt, Ge, Lt, Le };

  auto perform_op(const char* name, Inst* inst, OpKind op) -> bool {
    if (m_stack_.size() < 2) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = inst->source,
          .message = std::format("'{}' instruction expects 2 values on the stack, found {}", name,
                                 m_stack_.size()),
          .notes = {},
      });

      m_exit_ = true;
      return false;
    }

    size_t b{ m_stack_.top() };
    m_stack_.pop();
    size_t a{ m_stack_.top() };
    m_stack_.pop();

    switch (op) {
      case OpKind::Add:
        m_stack_.push(a + b);
        return true;
      case OpKind::Sub:
        m_stack_.push(a - b);
        return true;
      case OpKind::Mul:
        m_stack_.push(a * b);
        return true;
      case OpKind::Div:
        m_stack_.push(a / b);
        return true;
      case OpKind::Eq:
        m_stack_.push(a == b);
        return true;
      case OpKind::Ne:
        m_stack_.push(a != b);
        return true;
      case OpKind::Gt:
        m_stack_.push(a > b);
        return true;
      case OpKind::Ge:
        m_stack_.push(a >= b);
        return true;
      case OpKind::Lt:
        m_stack_.push(a < b);
        return true;
      case OpKind::Le:
        m_stack_.push(a <= b);
        return true;
    }

    return false;
  }

  auto interpret_inst(Inst* p_inst) -> bool {
    return std::visit(
        Overload{
            [&](const PushInst& inst) -> bool {
              m_stack_.push(inst.number);
              return true;
            },
            [&](const PopInst&) -> bool {
              bool con{ true };
              expect_val("pop", p_inst, &con);
              return con;
            },
            [&](const StoreInst& inst) -> bool {
              bool   con{ true };
              size_t val{ expect_val("store", p_inst, &con) };
              m_variables_[inst.identifier] = val;
              return con;
            },
            [&](const LoadInst& inst) -> bool {
              const auto it{ m_variables_.find(inst.identifier) };
              if (it == m_variables_.end()) {
                Diagnostics::report(Diagnostic{
                    .severity = DiagnosticSeverity::Fatal,
                    .range = p_inst->source,
                    .message = std::format("variable '{}' not stored yet", inst.identifier,
                                           m_stack_.size()),
                    .notes = {},
                });

                m_exit_ = true;
                return false;
              }

              m_stack_.push(it->second);
              return true;
            },
            [&](const JmpInst& inst) -> bool { return execute_branch(p_inst, inst.branch, false); },
            [&](const JmpTInst& inst) -> bool {
              bool   con{ true };
              size_t val{ expect_val("jmp_t", p_inst, &con) };
              if (!con)
                return false;
              if (val == 0)
                return true;
              return execute_branch(p_inst, inst.branch, false);
            },
            [&](const JmpFInst& inst) -> bool {
              bool   con{ true };
              size_t val{ expect_val("jmp_f", p_inst, &con) };
              if (!con)
                return false;
              if (val != 0)
                return true;
              return execute_branch(p_inst, inst.branch, false);
            },
            [&](const CallInst& inst) -> bool { return execute_branch(p_inst, inst.branch); },
            [&](const CallTInst& inst) -> bool {
              bool   con{ true };
              size_t val{ expect_val("call_t", p_inst, &con) };
              if (!con)
                return false;
              if (val == 0)
                return true;
              return execute_branch(p_inst, inst.branch);
            },
            [&](const CallFInst& inst) -> bool {
              bool   con{ true };
              size_t val{ expect_val("call_f", p_inst, &con) };
              if (!con)
                return false;
              if (val != 0)
                return true;
              return execute_branch(p_inst, inst.branch);
            },
            [&](const AllocInst&) -> bool {
              bool   con{ true };
              size_t val{ expect_val("alloc", p_inst, &con) };
              if (con) {
                auto* addr = static_cast<uint8_t*>(malloc(static_cast<size_t>(val)));
                m_stack_.push(reinterpret_cast<size_t>(addr));
              }
              return con;
            },
            [&](const FreeInst&) -> bool {
              bool   con{ true };
              size_t val{ expect_val("free", p_inst, &con) };
              if (con) {
                // NOLINTNEXTLINE
                free(reinterpret_cast<uint8_t*>(val));
              }
              return con;
            },
            [&](const StoreAddrInst& inst) -> bool {
              bool   con{ true };
              size_t b{ 0 };
              size_t a{ expect_2val("store_addr", p_inst, &con, &b) };
              if (!con)
                return false;

              const auto it{ m_variables_.find(inst.identifier) };
              if (it == m_variables_.end()) {
                Diagnostics::report(Diagnostic{
                    .severity = DiagnosticSeverity::Fatal,
                    .range = p_inst->source,
                    .message = std::format("variable '{}' not stored yet", inst.identifier,
                                           m_stack_.size()),
                    .notes = {},
                });

                m_exit_ = true;
                return false;
              }

              // NOLINTNEXTLINE
              *(reinterpret_cast<uint8_t*>(it->second) + a) = static_cast<uint8_t>(b);
              return con;
            },
            [&](const LoadAddrInst&) -> bool {
              bool   con{ true };
              size_t val{ expect_val("load_addr", p_inst, &con) };
              if (!con)
                return false;
              // NOLINTNEXTLINE
              m_stack_.push(*reinterpret_cast<uint8_t*>(val));
              return true;
            },
            [&](const DupInst&) -> bool {
              bool   con{ true };
              size_t val{ expect_val("dup", p_inst, &con, false) };
              if (con)
                m_stack_.push(val);
              return con;
            },
            [&](const ExitInst&) -> bool {
              bool   con{ true };
              size_t val{ expect_val("exit", p_inst, &con) };
              m_exit_ = true;
              if (con)
                m_exitCode_ = std::optional{ val };
              return false;
            },
            [&](const DumpDInst&) -> bool {
              bool   con{ true };
              size_t val{ expect_val("dump_int", p_inst, &con, false) };
              if (con)
                std::print("{:d}", val);
              return con;
            },
            [&](const DumpCInst&) -> bool {
              bool   con{ true };
              size_t val{ expect_val("dump_char", p_inst, &con, false) };
              if (con)
                std::print("{:c}", val);
              return con;
            },
            [&](const AddInst&) -> bool { return perform_op("add", p_inst, OpKind::Add); },
            [&](const SubInst&) -> bool { return perform_op("sub", p_inst, OpKind::Sub); },
            [&](const MulInst&) -> bool { return perform_op("mul", p_inst, OpKind::Mul); },
            [&](const DivInst&) -> bool { return perform_op("div", p_inst, OpKind::Div); },
            [&](const EqInst&) -> bool { return perform_op("eq", p_inst, OpKind::Eq); },
            [&](const NeInst&) -> bool { return perform_op("ne", p_inst, OpKind::Ne); },
            [&](const LtInst&) -> bool { return perform_op("lt", p_inst, OpKind::Lt); },
            [&](const LeInst&) -> bool { return perform_op("le", p_inst, OpKind::Le); },
            [&](const GtInst&) -> bool { return perform_op("gt", p_inst, OpKind::Gt); },
            [&](const GeInst&) -> bool { return perform_op("ge", p_inst, OpKind::Ge); },
        },
        p_inst->variant);
  }

  void interpret_branch(Branch* branch) {
    for (Inst* inst : branch->insts) {
      if (m_exit_ || !interpret_inst(inst))
        break;
    }
  }

 private:
  bool                  m_exit_{ false };
  std::optional<size_t> m_exitCode_{ std::nullopt };

  std::stack<size_t>                            m_stack_;
  std::unordered_map<std::string_view, size_t>  m_variables_;
  std::unordered_map<std::string_view, Branch*> m_branches_;
};

#endif  // JLD_MCC_IR_INTERPRETER_H