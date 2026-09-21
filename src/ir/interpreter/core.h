// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INTERPRETER_H
#define JLD_MCC_IR_INTERPRETER_H

#include <cassert>
#include <cstdint>
#include <cstdio>
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
#include "ir/insts.h"
#include "ir/interpreter/allocator.h"
#include "ir/syntaxer.h"
#include "ir/tokenizer.h"
#include "overloader.h"
#include "type_arena.h"
#include "types.h"
#include "utility.h"
#include "values.h"

/*

TODOs:

- add type safety to store
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
      , m_allocator_{ &m_exit_ }
      , m_flowHandler_{ &m_exit_, m_allocator_ } {}

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

    IrFunction*       entry_function{ nullptr };
    InterpreterResult res{ InterpreterResult::Error };

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
    } else {
      std::println("\033[1m\033[94m<interpreter>\033[m: executing '{}':\n", filepath);
      res = call_function(entry_function, entry_function->identRange);
    }

    assert(m_flowHandler_.branch_stack().empty());

    if (res == InterpreterResult::ExitCalled)
      std::println("\n\n\033[1m\033[94m<interpreter>\033[m: exited with code {}",
                   m_exitCode_.value());
    else {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Fatal,
          .range = SourceMultiRange{},
          .message = Diagnostics::format(
              "/Bprogram didn't exit correctly/R (no exit instruction was executed)"),
          .notes = {},
      });
    }

    Diagnostics::print();
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
      -> const IrInterpIntVal* {
    const IrInterpIntVal* val_int = std::get_if<IrInterpIntVal>(&val->data);
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

  [[nodiscard]] auto execute_inst(IrInst* p_inst) -> InterpreterResult {
    return std::visit(
        Overload{
            [&](const DeclareInst& inst) -> InterpreterResult {
              return m_flowHandler_.declare_local(inst.ident, inst.type, p_inst->instRange);
            },
            [&](const StoreInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();
                return m_flowHandler_.store_slot(inst.dest, arg0, p_inst->instRange);
              }

              return InterpreterResult::Error;
            },
            [&](const ArithmeticInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0, inst.arg1); args.has_value()) {
                auto [arg0, arg1] = args.value();

                IrValue res = m_overloader_.arithmetic_op(inst.kind, arg0, arg1);
                if (res.is_error())
                  return InterpreterResult::Error;
                return m_flowHandler_.store_slot(inst.dest, res, p_inst->instRange);
              }

              return InterpreterResult::Error;
            },
            [&](const ComparisionInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0, inst.arg1); args.has_value()) {
                auto [arg0, arg1] = args.value();

                bool res = m_overloader_.comparision_op(inst.kind, arg0, arg1);
                return m_flowHandler_.store_slot(
                    inst.dest,
                    IrValue{
                        .data = IrInterpIntVal{ res, IrIntegerType::U1 },
                        .type = m_types_.emplace_char(),
                    },
                    p_inst->instRange);
              }

              return InterpreterResult::Error;
            },
            [&](const JumpInst& inst) -> InterpreterResult {
              IrLabel* label = m_flowHandler_.label(inst.dest, inst.destRange);
              if (label == nullptr)
                return InterpreterResult::Error;
              if (!m_flowHandler_.in_label())
                return jump_label(label);

              m_jumpTo_ = label;
              return InterpreterResult::ExitLabel;
            },
            [&](const CallInst& inst) -> InterpreterResult {
              IrFunction* function = m_flowHandler_.function(inst.dest, inst.destRange);
              if (function == nullptr)
                return InterpreterResult::Error;
              return call_function(function, p_inst->instRange);
            },
            [&](const BranchIfInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0, inst.arg2); args.has_value()) {
                auto [arg0, arg2] = args.value();

                bool res = m_overloader_.comparision_op(inst.arg1, arg0, arg2);
                if (!res)
                  return InterpreterResult::Success;

                if (inst.kind == BranchIfInstKind::Jump) {
                  IrLabel* label = m_flowHandler_.label(inst.dest, inst.destRange);
                  if (label == nullptr)
                    return InterpreterResult::Error;
                  if (!m_flowHandler_.in_label()) {
                    InterpreterResult res = jump_label(label);
                    if (res != InterpreterResult::Success)
                      return res;
                    return InterpreterResult::ExitFunction;
                  }

                  m_jumpTo_ = label;
                  return InterpreterResult::ExitLabel;
                }

                IrFunction* function = m_flowHandler_.function(inst.dest, inst.destRange);
                if (function != nullptr)
                  return call_function(function, p_inst->instRange);
              }

              return InterpreterResult::Error;
            },
            [&](const AllocInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrInterpIntVal* arg0_int =
                    expect_integer("alloc", "first", arg0, inst.arg0.argRange);
                if (arg0_int == nullptr)
                  return InterpreterResult::Error;

                IrHeapAllocMetadata* alloc =
                    m_allocator_.alloc_heap(arg0_int->val, p_inst->instRange);

                return m_flowHandler_.store_slot(inst.dest,
                                                 IrValue{
                                                     .data =
                                                         IrPointerValue{
                                                             .alloc = { .heap = alloc },
                                                             .offset = 0,
                                                             .size = 0,
                                                             .origin = IrPointerOrigin::Heap,
                                                             .refCounted = false,
                                                         },
                                                     .type = nullptr,
                                                 },
                                                 p_inst->instRange);
              }

              return InterpreterResult::Error;
            },
            [&](const FreeInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrPointerValue* arg0_ptr =
                    expect_pointer("free", "first", arg0, inst.arg0.slot.identRange);
                return arg0_ptr == nullptr ? InterpreterResult::Error
                                           : m_allocator_.free_heap(arg0_ptr->alloc.heap,
                                                                    inst.arg0.slot.identRange);
              }

              return InterpreterResult::Error;
            },
            [&](const StoreAtInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0, inst.dest, inst.dest1); args.has_value()) {
                auto [arg0, dest, dest1] = args.value();

                const IrInterpIntVal* arg0_int =
                    expect_integer("store_at", "first", arg0, inst.arg0.argRange);
                if (arg0_int == nullptr)
                  return InterpreterResult::Error;

                const IrPointerValue* dest_ptr =
                    expect_pointer("store_at", "destination", dest, inst.dest.slot.identRange);
                if (dest_ptr == nullptr)
                  return InterpreterResult::Error;

                const IrInterpIntVal* dest1_int =
                    expect_integer("store_at", "second destination", dest1, inst.dest1.argRange);
                if (dest1_int == nullptr)
                  return InterpreterResult::Error;

                std::uintptr_t addr{ m_allocator_.access_heap(dest_ptr, dest1_int->val, "store_at",
                                                              inst.dest.slot.identRange) };
                if (addr == 0)
                  return InterpreterResult::Error;

                // NOLINTNEXTLINE
                *(reinterpret_cast<char*>(addr) + dest1_int->val) =
                    static_cast<char>(arg0_int->val);

                return InterpreterResult::Success;
              }

              return InterpreterResult::Error;
            },
            [&](const StoreAddrInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                return m_flowHandler_.store_slot(inst.dest,
                                                 IrValue{
                                                     .data =
                                                         IrPointerValue{
                                                             .alloc = {
                                                              .stack = arg0,
                                                             },
                                                             .offset = 0,
                                                             .size = 8,
                                                             .origin = IrPointerOrigin::Stack,
                                                             .refCounted = false,
                                                         },
                                                     .type = nullptr,
                                                 },
                                                 p_inst->instRange);
              }

              return InterpreterResult::Error;
            },
            [&](const LoadAddrInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrPointerValue* arg0_ptr =
                    expect_pointer("load_addr", "first", arg0, inst.arg0.slot.identRange);

                if (arg0_ptr == nullptr)
                  return InterpreterResult::Error;

                auto* ptr_type = std::get_if<PointerType>(&arg0->type->data);

                if (arg0_ptr->origin == IrPointerOrigin::Stack) {
                  const IrValue* pointee{ m_allocator_.access_stack(arg0_ptr,
                                                                    inst.arg0.slot.identRange) };
                  if (pointee == nullptr)
                    return InterpreterResult::Error;
                  return m_flowHandler_.store_slot(inst.dest, pointee, p_inst->instRange);
                }

                std::uintptr_t addr{ m_allocator_.access_heap(arg0_ptr, "load_addr",
                                                              inst.arg0.slot.identRange) };
                if (addr == 0)
                  return InterpreterResult::Error;

                return std::visit(Overload{ [&](BuiltinType type) -> InterpreterResult {
                                             switch (type.kind) {
                                               case BuiltinTypeKind::U8:
                                                 return m_flowHandler_.store_slot(
                                                     inst.dest,
                                                     IrValue{
                                                         .data =
                                                             IrInterpIntVal{
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
                                                             IrInterpIntVal{
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
                                             return InterpreterResult::Error;
                                           },
                                            [&](PointerType&) -> InterpreterResult {
                                              m_exit_ = true;
                                              assert(false);
                                              return InterpreterResult::Error;
                                            } },
                                  ptr_type->pointee->data);
              }

              return InterpreterResult::Error;
            },
            [&](const RetInst& inst) -> InterpreterResult {
              m_flowHandler_.clear_ret(
                  source_multi_range_from(p_inst->instRange, inst.args.back().argRange));

              for (uint32_t i = 0; i < inst.args.size(); ++i) {
                const InstArg& arg = inst.args[i];
                const IrValue* arg_val = interpret_arg(arg);

                if (arg_val == nullptr) {
                  m_exit_ = true;
                  return InterpreterResult::Error;
                }
                m_flowHandler_.emplace_ret(arg_val, arg.argRange);
              }

              return InterpreterResult::Success;
            },
            [&](const ExitInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                const IrInterpIntVal* arg0_int =
                    expect_integer("exit", "first", arg0, inst.arg0.argRange);
                if (arg0_int == nullptr)
                  return InterpreterResult::Error;

                m_exitCode_ = std::optional{ arg0_int->val };
                return InterpreterResult::ExitCalled;
              }

              return InterpreterResult::Error;
            },
            [&](const PrintInst& inst) -> InterpreterResult {
              if (auto args = check_args(inst.arg0); args.has_value()) {
                auto [arg0] = args.value();

                return std::visit(
                    Overload{ [](IrInterpIntVal value) -> InterpreterResult {
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

                               return InterpreterResult::Success;
                             },
                              [](IrPointerValue value) -> InterpreterResult {
                                if (value.origin == IrPointerOrigin::Stack) {
                                  std::println("{:x}", reinterpret_cast<size_t>(value.alloc.stack));
                                  return InterpreterResult::Success;
                                }

                                std::println("{:x}", value.alloc.heap->base + value.offset);
                                return InterpreterResult::Success;
                              },
                              [](IrErrorValue) -> InterpreterResult {
                                std::println("Error IR value");
                                return InterpreterResult::Success;
                              },
                              [](IrUndeclaredValue) -> InterpreterResult {
                                std::println("Undeclared IR value");
                                return InterpreterResult::Success;
                              } },
                    arg0->data);
              }

              return InterpreterResult::Error;
            },
        },
        p_inst->data);
  }

  [[nodiscard]] auto jump_label(IrLabel* label) -> InterpreterResult {
    m_flowHandler_.enter_label(label);

    for (IrInst* inst : label->insts) {
      InterpreterResult res = execute_inst(inst);

      if (res == InterpreterResult::Error || res == InterpreterResult::ExitCalled) {
        if (m_flowHandler_.exit_func_or_label() == InterpreterResult::Error)
          return InterpreterResult::Error;
        return res;
      }

      if (res == InterpreterResult::ExitFunction)
        break;

      if (res == InterpreterResult::ExitLabel) {
        if (m_flowHandler_.exit_func_or_label() == InterpreterResult::Error)
          return InterpreterResult::Error;
        return jump_label(m_jumpTo_);
      }
    }

    return m_flowHandler_.exit_func_or_label();
  }

  [[nodiscard]] auto call_function(IrFunction* function, const SourceRange& call_range)
      -> InterpreterResult {
    if (m_flowHandler_.enter_function(function, call_range) != InterpreterResult::Success)
      return InterpreterResult::Error;

    for (IrInst* inst : function->insts) {
      InterpreterResult res = execute_inst(inst);

      if (res == InterpreterResult::Error || res == InterpreterResult::ExitCalled) {
        if (m_flowHandler_.exit_func_or_label() == InterpreterResult::Error)
          return InterpreterResult::Error;
        return res;
      }

      if (res == InterpreterResult::ExitFunction)
        break;

      if (res == InterpreterResult::ExitLabel)
        assert(false);
    }

    return m_flowHandler_.exit_func_or_label();
  }

 private:
  TypeArena&             m_types_;
  IrOverloader           m_overloader_;
  IrInterpreterAllocator m_allocator_;

  IrLabel* m_jumpTo_{ nullptr };

  bool                   m_exit_{ false };
  std::optional<int32_t> m_exitCode_{ std::nullopt };

  IrFlowHandler m_flowHandler_;
};

#endif  // JLD_MCC_IR_INTERPRETER_H