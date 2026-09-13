// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INTERPRETER_H
#define JLD_MCC_IR_INTERPRETER_H

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

  enum class LookupKind : uint8_t {
    Local,
    Param,
    Ret,
  };

  auto lookup_inst_parameter(InstParameter param) -> size_t {
    return std::visit(
        Overload{
            [&](std::string_view identifier) -> size_t { return m_variables_[identifier]; },
            [](size_t constant) -> size_t { return constant; },
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

  auto interpret_inst(Inst* p_inst) -> bool {
    return std::visit(
        Overload{
            [&](const StoreInst& inst) -> bool {
              size_t val{ lookup_inst_parameter(inst.a) };
              m_variables_[inst.toVar] = val;
              return true;
            },
            [&](const LoadInst& inst) -> bool {
              m_variables_[inst.toVar] = m_variables_[inst.a];
              return true;
            },
            [&](const ArithmeticInst& inst) -> bool {
              size_t a{ lookup_inst_parameter(inst.a) };
              size_t b{ lookup_inst_parameter(inst.b) };

              switch (inst.kind) {
                case ArithmeticInstKind::Add:
                  m_variables_[inst.toVar] = a + b;
                  return true;
                case ArithmeticInstKind::Sub:
                  m_variables_[inst.toVar] = a - b;
                  return true;
                case ArithmeticInstKind::Mul:
                  m_variables_[inst.toVar] = a * b;
                  return true;
                case ArithmeticInstKind::Div:
                  m_variables_[inst.toVar] = a / b;
                  return true;
              }
              return false;
            },
            [&](const ComparisionInst& inst) -> bool {
              size_t a{ lookup_inst_parameter(inst.a) };
              size_t b{ lookup_inst_parameter(inst.b) };
              m_variables_[inst.toVar] = comparision_result(a, b, inst.kind);
              return true;
            },
            [&](const BranchInst& inst) -> bool {
              return execute_branch(p_inst, inst.toBranch, inst.kind != BranchInstKind::Jmp);
            },
            [&](const BranchIfInst& inst) -> bool {
              size_t a{ lookup_inst_parameter(inst.a) };
              size_t b{ lookup_inst_parameter(inst.b) };
              if (!comparision_result(a, b, inst.cmpKind))
                return true;
              return execute_branch(p_inst, inst.toBranch, inst.kind != BranchInstKind::Jmp);
            },
            [&](const AllocInst& inst) -> bool {
              size_t a{ lookup_inst_parameter(inst.a) };
              auto*  addr{ static_cast<uint8_t*>(malloc(a)) };
              m_variables_[inst.toVar] = reinterpret_cast<size_t>(addr);
              return true;
            },
            [&](const FreeInst& inst) -> bool {
              size_t val{ m_variables_[inst.identifier] };
              // NOLINTNEXTLINE
              free(reinterpret_cast<uint8_t*>(val));
              return true;
            },
            [&](const StorePtrInst& inst) -> bool {
              size_t a{ lookup_inst_parameter(inst.a) };
              size_t b{ lookup_inst_parameter(inst.b) };
              // NOLINTNEXTLINE
              uint8_t* ptr{ reinterpret_cast<uint8_t*>(m_variables_[inst.toVar]) };
              *(ptr + b) = static_cast<uint8_t>(a);
              return true;
            },
            [&](const StoreAddrInst& inst) -> bool {
              m_variables_[inst.toVar] = reinterpret_cast<size_t>(&m_variables_[inst.a]);
              return true;
            },
            [&](const LoadAddrInst& inst) -> bool {
              m_variables_[inst.toVar] =
                  // NOLINTNEXTLINE
                  static_cast<size_t>(*reinterpret_cast<uint8_t*>(m_variables_[inst.a]));
              return true;
            },
            [&](const ExitInst& inst) -> bool {
              size_t a{ lookup_inst_parameter(inst.a) };
              m_exit_ = true;
              m_exitCode_ = std::optional{ a };
              return false;
            },
            [&](const DumpInst& inst) -> bool {
              size_t a{ lookup_inst_parameter(inst.a) };
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

  void interpret_branch(Branch* branch) {
    for (Inst* inst : branch->insts) {
      if (m_exit_ || !interpret_inst(inst))
        break;
    }
  }

 private:
  bool                  m_exit_{ false };
  std::optional<size_t> m_exitCode_{ std::nullopt };

  std::unordered_map<std::string_view, Branch*> m_branches_;
  std::unordered_map<std::string_view, size_t>  m_variables_;
};

#endif  // JLD_MCC_IR_INTERPRETER_H