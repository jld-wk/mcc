// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INTERPRETER_H
#define JLD_MCC_IR_INTERPRETER_H

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "diagnostic/core.h"
#include "diagnostic/source.h"
#include "diagnostic/source_manager.h"
#include "flow_handler.h"
#include "insts.h"
#include "overloader.h"
#include "syntaxer.h"
#include "tokenizer.h"
#include "type_arena.h"
#include "types.h"
#include "utility.h"
#include "values.h"

/*

TODOs:

- Add a cast keyword (e.g. char to int) and a function to do implicit conversions and then return,
but add safety!
- Handle any TODO
- Make alloc type safe with something like: alloc 4 -> int* [ptr]
- Check free pointers (double-free's/invalid ptrs)
- Make exit inst expected a uint8?
- Handle every assert
- Store at should offset using the offset and type (1 -> 1 * sizeof(type))
- print should have another argument that says how to print, like: asc (as char), asd (as decimal)
- add explicit println instruction

*/
class IrInterpreter {
 public:
  explicit IrInterpreter(TypeArena& types)
      : m_types_{ types }
      , m_overloader_{ &m_exit_ }
      , m_flowHandler_{ &m_exit_ } {}

  void interpret_file(const std::string& filepath, SourceManager& manager) {
    FileId           file{ manager.open(filepath) };
    std::string_view source{ manager.find(file).source };

    IrTokenizer          tokenizer;
    std::vector<IrToken> tokens{ tokenizer.tokenize(file, source) };

    for (const IrToken& token : tokens)
      std::println("{} -> {}", format_ir_token_kind(token.kind), token.text);
    std::println("");

    IrSyntaxer               syntaxer{ tokens, m_types_ };
    std::vector<IrFunction*> functions{ syntaxer.build() };

    if (Diagnostics::error_count() > 0) {
      Diagnostics::print();
      return;
    }

    IrFunction* entry_function{ nullptr };

    for (IrFunction* function : functions) {
      if (function->ident == "entry")
        entry_function = function;
      m_flowHandler_.add_function(function);
    }

    if (entry_function == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = SourceMultiRange{},
          .message = Diagnostics::format("/Bentry function not found/R"),
          .notes = {},
      });
    } else
      call_function(entry_function, entry_function->identRange);

    if (m_exitCode_.has_value())
      std::println("[Interpreter] Program exited with code {}", m_exitCode_.value());
    else {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = SourceMultiRange{},
          .message = Diagnostics::format(
              "/Bprogram didn't exit correctly/R (no exit instruction was executed)"),
          .notes = {},
      });
      Diagnostics::print();
    }
  }

 private:
  [[nodiscard]] auto interpret_arg(const InstArg& arg) -> const IrValue* {
    return std::visit(Overload{
                          [&](const IrValue& val) -> const IrValue* { return &val; },
                          [&](const IrUntypedSlot& slot) -> const IrValue* {
                            return m_flowHandler_.find_slot(slot, arg.argRange);
                          },
                      },
                      arg.data);
  }

  template <typename... Args>
  JLD_MCC_FORCE_INLINE [[nodiscard]] auto check_args(Args&... args) -> auto {
    auto arg_op = [&]<typename ArgType>(ArgType& arg) -> const IrValue* {
      if constexpr (std::is_same_v<std::decay_t<ArgType>, InstArg>)
        return interpret_arg(arg);
      else
        return m_flowHandler_.find_slot(arg.slot, arg.typeRange);
    };

    auto values = std::make_tuple(arg_op(args)...);
    bool valid = std::apply([](auto*... ptrs) -> auto { return (ptrs && ...); }, values);

    using RetType = decltype(values);
    if (!valid) {
      m_exit_ = true;
      return std::optional<RetType>{ std::nullopt };
    }
    return std::optional<RetType>{ values };
  }

  [[nodiscard]] auto expect_integer(const char* inst_name, const char* argument_ident,
                                    const IrValue* val, const SourceRange& val_range)
      -> const IrIntegerValue* {
    const IrIntegerValue* val_int = std::get_if<IrIntegerValue>(&val->data);
    if (val_int == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(val_range),
          .message = Diagnostics::format("/B'{}'/R instruction expects the /B{} argument/R to be "
                                         "an /Binteger/R, but received a /B'{}'/R type",
                                         inst_name, argument_ident, val->type->format()),
          .notes = {},
      });

      m_exit_ = true;
      return nullptr;
    }

    return val_int;
  }

  [[nodiscard]] auto expect_pointer(const char* inst_name, const char* argument_ident,
                                    const IrValue* val, const SourceRange& val_range)
      -> const IrPointerValue* {
    const IrPointerValue* val_int = std::get_if<IrPointerValue>(&val->data);
    if (val_int == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(val_range),
          .message = Diagnostics::format("/B'{}'/R instruction expects the /B{} argument/R to be "
                                         "a /Bpointer/R, but received a /B'{}'/R type",
                                         inst_name, argument_ident, val->type->format()),
          .notes = {},
      });

      m_exit_ = true;
      return nullptr;
    }

    return val_int;
  }

  [[nodiscard]] auto interpret_inst(IrInst* p_inst) -> bool {
    return std::visit(
        Overload{
            [&](const DeclareInst& inst) -> bool {
              return m_flowHandler_.declare_local(inst.ident, inst.type, p_inst->instRange);
            },
            [&](const StoreInst& inst) -> bool {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();
                return m_flowHandler_.store_slot(inst.dest, arg0, p_inst->instRange);
              }

              return false;
            },
            [&](const ArithmeticInst& inst) -> bool {
              if (auto args = check_args(inst.arg0, inst.arg1); args.has_value()) {
                auto [arg0, arg1] = args.value();

                IrValue res = m_overloader_.arithmetic_op(inst.kind, arg0, arg1);
                if (res.is_error())
                  return false;
                return m_flowHandler_.store_slot(inst.dest, res, p_inst->instRange);
              }

              return false;
            },
            [&](const ComparisionInst& inst) -> bool {
              if (auto args = check_args(inst.arg0, inst.arg1); args.has_value()) {
                auto [arg0, arg1] = args.value();

                bool res = m_overloader_.comparision_op(inst.kind, arg0, arg1);
                return m_flowHandler_.store_slot(
                    inst.dest,
                    IrValue{
                        .data = IrIntegerValue{ res, IrIntegerType::U1 },
                        .type = m_types_.emplace_char(),
                    },
                    p_inst->instRange);
              }

              return false;
            },
            [&](const JumpInst& inst) -> bool {
              IrLabel* label = m_flowHandler_.label(inst.dest, inst.destRange);
              if (label == nullptr)
                return false;
              jump_label(label);
              return true;
            },
            [&](const CallInst& inst) -> bool {
              IrFunction* function = m_flowHandler_.function(inst.dest, inst.destRange);
              if (function == nullptr)
                return false;
              call_function(function, p_inst->instRange);
              return true;
            },
            [&](const BranchIfInst& inst) -> bool {
              if (auto args = check_args(inst.arg0, inst.arg2); args.has_value()) {
                auto [arg0, arg2] = args.value();

                bool res = m_overloader_.comparision_op(inst.arg1, arg0, arg2);
                if (!res)
                  return true;

                if (inst.kind == BranchIfInstKind::Jump) {
                  IrLabel* label = m_flowHandler_.label(inst.dest, inst.destRange);
                  if (label == nullptr)
                    return false;
                  jump_label(label);
                  return true;
                }

                IrFunction* function = m_flowHandler_.function(inst.dest, inst.destRange);
                if (function != nullptr) {
                  call_function(function, p_inst->instRange);
                  return true;
                }
              }

              return false;
            },
            [&](const AllocInst& inst) -> bool {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrIntegerValue* arg0_int =
                    expect_integer("alloc", "first", arg0, inst.arg0.argRange);
                if (arg0_int == nullptr)
                  return false;

                std::uintptr_t addr{ reinterpret_cast<std::uintptr_t>(malloc(arg0_int->val)) };
                return m_flowHandler_.store_slot(
                    inst.dest,
                    IrValue{
                        .data = IrPointerValue{ .addr = addr },
                        .type = m_types_.emplace_ptr(m_types_.emplace_char()),
                    },
                    p_inst->instRange);
              }

              return false;
            },
            [&](const FreeInst& inst) -> bool {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrPointerValue* arg0_ptr =
                    expect_pointer("free", "first", arg0, inst.arg0.slot.identRange);
                if (arg0_ptr == nullptr)
                  return false;

                std::uintptr_t addr = arg0_ptr->addr;
                // NOLINTNEXTLINE
                free(reinterpret_cast<void*>(addr));
                return true;
              }

              return false;
            },
            [&](const StoreAtInst& inst) -> bool {
              if (auto args = check_args(inst.arg0, inst.dest, inst.dest1); args.has_value()) {
                auto [arg0, dest, dest1] = args.value();

                const IrIntegerValue* arg0_int =
                    expect_integer("store_at", "first", arg0, inst.arg0.argRange);
                if (arg0_int == nullptr)
                  return false;

                const IrPointerValue* dest_ptr =
                    expect_pointer("store_at", "destination", dest, inst.dest.slot.identRange);
                if (dest_ptr == nullptr)
                  return false;

                const IrIntegerValue* dest1_int =
                    expect_integer("store_at", "second destination", dest1, inst.dest1.argRange);
                if (dest1_int == nullptr)
                  return false;

                // NOLINTNEXTLINE
                *(reinterpret_cast<char*>(dest_ptr->addr) + dest1_int->val) =
                    static_cast<char>(arg0_int->val);

                return true;
              }

              return false;
            },
            [&](const StoreAddrInst& inst) -> bool {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                return m_flowHandler_.store_slot(
                    inst.dest,
                    IrValue{
                        .data =
                            IrPointerValue{
                                .addr = reinterpret_cast<std::uintptr_t>(arg0),
                            },
                        .type = m_types_.emplace_ptr(arg0->type),
                    },
                    p_inst->instRange);
              }

              return false;
            },
            [&](const LoadAddrInst& inst) -> bool {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrPointerValue* arg0_ptr =
                    expect_pointer("load_addr", "first", arg0, inst.arg0.slot.identRange);
                if (arg0_ptr == nullptr)
                  return false;

                std::uintptr_t addr = arg0_ptr->addr;
                auto*          ptr_type = std::get_if<PointerType>(&arg0->type->data);

                return std::visit(
                    Overload{ [&](BuiltinType type) -> bool {
                               switch (type.kind) {
                                 case BuiltinTypeKind::U8:
                                   return m_flowHandler_.store_slot(
                                       inst.dest,
                                       IrValue{
                                           .data =
                                               IrIntegerValue{
                                                   // NOLINTNEXTLINE
                                                   *reinterpret_cast<uint8_t*>(addr),
                                                   IrIntegerType::U8,
                                               },
                                           .type = ptr_type->pointee,
                                       },
                                       p_inst->instRange);
                                 case BuiltinTypeKind::U32:
                                   return m_flowHandler_.store_slot(
                                       inst.dest,
                                       IrValue{
                                           .data =
                                               IrIntegerValue{
                                                   // NOLINTNEXTLINE
                                                   *reinterpret_cast<uint32_t*>(addr),
                                                   IrIntegerType::U32,
                                               },
                                           .type = ptr_type->pointee,
                                       },
                                       p_inst->instRange);
                                 case BuiltinTypeKind::Error:
                                   break;
                               }

                               m_exit_ = true;
                               assert(false);
                               return false;
                             },
                              [&](PointerType) -> bool {
                                return m_flowHandler_.store_slot(
                                    inst.dest,
                                    IrValue{
                                        .data =
                                            IrPointerValue{
                                                // NOLINTNEXTLINE
                                                .addr = *reinterpret_cast<std::uintptr_t*>(addr),
                                            },
                                        .type = ptr_type->pointee,
                                    },
                                    p_inst->instRange);
                              } },
                    ptr_type->pointee->data);
              }

              return false;
            },
            [&](const RetInst& inst) -> bool {
              m_flowHandler_.clear_ret(
                  source_multi_range_from(p_inst->instRange, inst.args.back().argRange));

              // TODO(jld-wk): Limit? yes
              for (uint32_t i = 0; i < inst.args.size(); ++i) {
                const InstArg& arg = inst.args[i];
                const IrValue* arg_val = interpret_arg(arg);

                if (arg_val == nullptr) {
                  m_exit_ = true;
                  return false;
                }
                m_flowHandler_.emplace_ret(*arg_val, arg.argRange);
              }

              return true;
            },
            [&](const ExitInst& inst) -> bool {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrIntegerValue* arg0_int =
                    expect_integer("exit", "first", arg0, inst.arg0.argRange);
                if (arg0_int == nullptr)
                  return false;

                m_exitCode_ = std::optional{ arg0_int->val };
              }

              m_exit_ = true;
              return false;
            },
            [&](const PrintInst& inst) -> bool {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                return std::visit(Overload{ [](IrIntegerValue value) -> bool {
                                             switch (value.type) {
                                               case IrIntegerType::U1:
                                                 std::println("{}", value.val ? "true" : "false");
                                                 break;
                                               case IrIntegerType::U8:
                                                 std::print("{:c}", value.val);
                                                 break;
                                               case IrIntegerType::U16:
                                               case IrIntegerType::U32:
                                               case IrIntegerType::U64:
                                                 std::println("{}", value.val);
                                                 break;
                                             }

                                             return true;
                                           },
                                            [](IrPointerValue value) -> bool {
                                              std::println("{}", value.addr);
                                              return true;
                                            },
                                            [](IrErrorValue) -> bool {
                                              std::println("Error IR value");
                                              return true;
                                            },
                                            [](IrUndeclaredValue) -> bool {
                                              std::println("Undeclared IR value");
                                              return true;
                                            } },
                                  arg0->data);
              }

              return false;
            },
        },
        p_inst->data);
  }

  void jump_label(IrLabel* label) {
    m_flowHandler_.enter_label(label);

    for (IrInst* inst : label->insts) {
      if (!interpret_inst(inst) || m_exit_)
        break;
    }

    m_flowHandler_.exit_func_or_label();
  }

  void call_function(IrFunction* function, const SourceRange& call_range) {
    if (!m_flowHandler_.enter_function(function, call_range)) {
      m_exit_ = true;
      return;
    }

    for (IrInst* inst : function->insts) {
      if (!interpret_inst(inst) || m_exit_)
        break;
    }

    m_flowHandler_.exit_func_or_label();
  }

 private:
  TypeArena&   m_types_;
  IrOverloader m_overloader_;

  bool                   m_exit_{ false };
  std::optional<int32_t> m_exitCode_{ std::nullopt };

  IrFlowHandler m_flowHandler_;
};

#endif  // JLD_MCC_IR_INTERPRETER_H