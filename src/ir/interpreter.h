// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INTERPRETER_H
#define JLD_MCC_IR_INTERPRETER_H

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <print>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ir/insts.h"
#include "ir_syntaxer.h"
#include "ir_tokenizer.h"
#include "utility.h"

class IrInterpreter {
 public:
  void interpret_file(const std::string& filepath) {
    std::fstream stream{ filepath };
    stream.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);

    std::vector<char> buf(static_cast<size_t>(size) + 1);
    buf[static_cast<size_t>(size)] = '\0';
    stream.read(buf.data(), static_cast<std::streamsize>(size));

    std::string_view source{ buf.data(), size + 1 };

    IrTokenizer          tokenizer;
    std::vector<IrToken> tokens = tokenizer.tokenize(source);

    for (const IrToken& token : tokens)
      std::println("{} {} -> line(s/e): {}/{} | col(s/e): {}/{}", format_ir_token_kind(token.kind),
                   token.text, token.source.startLine, token.source.endLine,
                   token.source.startColumn, token.source.endColumn);
    std::println("");

    IrSyntaxer           syntaxer{ tokens };
    std::vector<Branch*> branches = syntaxer.build();

    Branch* main_branch = nullptr;

    for (Branch* branch : branches) {
      m_branches_[branch->identifier] = branch;
      if (branch->identifier == "main")
        main_branch = branch;
    }

    if (main_branch == nullptr) {
    }

    interpret_branch(main_branch);

    if (m_exitCode_ != -1)
      std::println("[Interpreter] Program exited with code {}", m_exitCode_);
    else
      std::println("[Interpreter] Program didn't exit correctly");
  }

 private:
  auto interpret_inst(Inst* p_inst) -> bool {
    return std::visit(Overload{ [&](const PushInst& inst) -> bool {
                                 m_stack_.push(inst.number);
                                 return true;
                               },
                                [&](const PopInst&) -> bool {
                                  m_stack_.pop();
                                  return true;
                                },
                                [&](const StoreInst& inst) -> bool {
                                  int val = m_stack_.top();
                                  m_stack_.pop();
                                  m_variables_[inst.identifier] = val;
                                  return true;
                                },
                                [&](const LoadInst& inst) -> bool {
                                  const auto it = m_variables_.find(inst.identifier);
                                  if (it == m_variables_.end()) {
                                    // TODO(jld-wk): error!
                                  }
                                  m_stack_.push(it->second);
                                  return true;
                                },
                                [&](const AddInst&) -> bool {
                                  int a = m_stack_.top();
                                  m_stack_.pop();
                                  int b = m_stack_.top();
                                  m_stack_.pop();
                                  m_stack_.push(a + b);
                                  return true;
                                },
                                [&](const SubInst&) -> bool {
                                  int a = m_stack_.top();
                                  m_stack_.pop();
                                  int b = m_stack_.top();
                                  m_stack_.pop();
                                  m_stack_.push(b - a);
                                  return true;
                                },
                                [&](const MulInst&) -> bool {
                                  int a = m_stack_.top();
                                  m_stack_.pop();
                                  int b = m_stack_.top();
                                  m_stack_.pop();
                                  m_stack_.push(a * b);
                                  return true;
                                },
                                [&](const DivInst&) -> bool {
                                  int a = m_stack_.top();
                                  m_stack_.pop();
                                  int b = m_stack_.top();
                                  m_stack_.pop();
                                  m_stack_.push(b / a);
                                  return true;
                                },
                                [&](const RetInst&) -> bool {
                                  m_exitCode_ = m_stack_.top();
                                  return false;
                                },
                                [&](const CallInst& inst) -> bool {
                                  const auto it = m_branches_.find(inst.branch);
                                  if (it == m_branches_.end()) {
                                    // TODO(jld-wk) error!
                                  }
                                  interpret_branch(it->second);
                                  return true;
                                },
                                [&](const CallTInst& inst) -> bool {
                                  int condition = m_stack_.top();
                                  if (condition == 0)
                                    return true;
                                  const auto it = m_branches_.find(inst.branch);
                                  if (it == m_branches_.end()) {
                                    // TODO(jld-wk) error!
                                  }
                                  interpret_branch(it->second);
                                  return true;
                                },
                                [&](const CallFInst& inst) -> bool {
                                  int condition = m_stack_.top();
                                  if (condition != 0)
                                    return true;
                                  const auto it = m_branches_.find(inst.branch);
                                  if (it == m_branches_.end()) {
                                    // TODO(jld-wk) error!
                                  }
                                  interpret_branch(it->second);
                                  return true;
                                },
                                [&](const JmpInst& inst) -> bool {
                                  const auto it = m_branches_.find(inst.branch);
                                  if (it == m_branches_.end()) {
                                    // TODO(jld-wk) error!
                                  }
                                  interpret_branch(it->second);
                                  return false;
                                },
                                [&](const JmpTInst& inst) -> bool {
                                  int condition = m_stack_.top();
                                  if (condition == 0)
                                    return true;
                                  const auto it = m_branches_.find(inst.branch);
                                  if (it == m_branches_.end()) {
                                    // TODO(jld-wk) error!
                                  }
                                  interpret_branch(it->second);
                                  return false;
                                },
                                [&](const JmpFInst& inst) -> bool {
                                  int condition = m_stack_.top();
                                  if (condition != 0)
                                    return true;
                                  const auto it = m_branches_.find(inst.branch);
                                  if (it == m_branches_.end()) {
                                    // TODO(jld-wk) error!
                                  }
                                  interpret_branch(it->second);
                                  return false;
                                },
                                [&](const DbgDumpInst&) -> bool {
                                  int val = m_stack_.top();
                                  std::println("{}", val);
                                  return true;
                                } },
                      p_inst->variant);
  }

  void interpret_branch(Branch* branch) {
    for (Inst* inst : branch->insts) {
      if (!interpret_inst(inst))
        break;
    }
  }

 private:
  int m_exitCode_{ -1 };

  std::stack<int>                               m_stack_;
  std::unordered_map<std::string_view, int>     m_variables_;
  std::unordered_map<std::string_view, Branch*> m_branches_;
};

#endif  // JLD_MCC_IR_INTERPRETER_H