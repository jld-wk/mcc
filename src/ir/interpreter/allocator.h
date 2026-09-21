// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_INTERPRETER_ALLOCATOR_H
#define JLD_MCC_IR_INTERPRETER_ALLOCATOR_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <variant>

#include "diagnostic/core.h"
#include "diagnostic/source.h"
#include "types.h"
#include "values.h"

class IrInterpreterAllocator {
 public:
  explicit IrInterpreterAllocator(bool* exit)
      : m_exit_{ exit } {}

  [[nodiscard]] auto alloc_stack(IrValueData&& data, Type* type, const SourceRange& alloc_range)
      -> IrValue* {
    auto* alloc = new IrStackAllocMetadata{ .range = alloc_range };
    auto* val_ptr = new IrValue{
      .data = data,
      .type = type,
    };

    auto addr = reinterpret_cast<std::uintptr_t>(val_ptr);
    m_stackAllocs_[addr] = alloc;
    return val_ptr;
  }

  [[nodiscard]] auto free_stack(const IrValue* var, SourceRange var_range) -> InterpreterResult {
    if (var == nullptr)
      return InterpreterResult::Success;

    auto        addr = reinterpret_cast<std::uintptr_t>(var);
    const auto& it = m_stackAllocs_.find(addr);
    if (it == m_stackAllocs_.end()) {
      assert(false);
      *m_exit_ = true;
      return InterpreterResult::Error;
    }

    const IrPointerValue* var_ptr = std::get_if<IrPointerValue>(&var->data);
    if (var_ptr != nullptr && var_ptr->origin == IrPointerOrigin::Heap) {
      IrHeapAllocMetadata* ptr_alloc = var_ptr->alloc.heap;
      if (ptr_alloc != nullptr && m_heapAllocs_.contains(ptr_alloc->base) &&
          (--ptr_alloc->refCount == 0)) {
        Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format("/Blast reference to memory/R that has been allocated on the heap /Bhas been implicitly destroyed/R, this will lead to a memory leak"),
          .notes = {
            Diagnostic{
              .severity = DiagnosticSeverity::Note,
              .range = static_cast<SourceMultiRange>(ptr_alloc->range),
              .message = Diagnostics::format("pointer points to /Bheap memory allocated here/R"),
              .notes = {},
            },
          },
      });

        *m_exit_ = true;
        delete var;
        delete it->second;
        return InterpreterResult::Error;
      }
    }

    delete var;
    delete it->second;

    m_stackAllocs_.erase(it);
    return InterpreterResult::Success;
  }

  [[nodiscard]] auto find_stack_alloc(const IrValue* var) -> IrStackAllocMetadata* {
    if (var == nullptr)
      return nullptr;

    auto        addr = reinterpret_cast<std::uintptr_t>(var);
    const auto& it = m_stackAllocs_.find(addr);
    return it == m_stackAllocs_.end() ? nullptr : it->second;
  }

  [[nodiscard]] auto alloc_heap(size_t size, const SourceRange& alloc_range)
      -> IrHeapAllocMetadata* {
    std::uintptr_t addr{ reinterpret_cast<std::uintptr_t>(malloc(size)) };
    auto*          alloc =
        new IrHeapAllocMetadata{ .base = addr, .size = size, .range = alloc_range, .refCount = 1 };
    m_heapAllocs_[addr] = alloc;
    return alloc;
  }

  [[nodiscard]] auto free_heap(IrHeapAllocMetadata* alloc, SourceRange free_range)
      -> InterpreterResult {
    if (alloc == nullptr)
      return InterpreterResult::Success;

    const auto& it = m_heapAllocs_.find(alloc->base);
    if (it == m_heapAllocs_.end()) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(free_range),
          .message = Diagnostics::format(
              "trying to free memory /Bwhich has been already freed/R (double-free)"),
          .notes = {},
      });

      *m_exit_ = true;
      return InterpreterResult::Error;
    }

    delete it->second;

    m_heapAllocs_.erase(it);
    return InterpreterResult::Success;
  }

  [[nodiscard]] auto access_heap(const IrPointerValue* var, size_t offset, const char* op_name,
                                 SourceRange var_range) -> std::uintptr_t {
    if (var->origin == IrPointerOrigin::Stack) {
      IrStackAllocMetadata* alloc = find_stack_alloc(var->alloc.stack);

      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format("trying to use this /Bstack pointer/R for a /Bheap-based "
                                         "pointer operation '{}'/R (disallowed)",
                                         op_name),
          .notes = {
            alloc == nullptr ? Diagnostic{
              .severity = DiagnosticSeverity::Note,
              .range = SourceMultiRange{},
              .message = Diagnostics::format("/Bpointer is invalid/R (it points to a stack-variable, that /Bhas been destroyed/R)"),
              .notes = {},
            } : Diagnostic{
              .severity = DiagnosticSeverity::Note,
              .range = static_cast<SourceMultiRange>(alloc->range),
              .message = Diagnostics::format("pointer points /Bto this stack variable/R"),
              .notes = {},
            },
          },
      });

      *m_exit_ = true;
      return 0;
    }

    if (var->alloc.heap == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format(
              "trying to access this heap pointer which /Bdoesn't point to any memory/R"),
          .notes = {},
      });

      *m_exit_ = true;
      return 0;
    }

    if (!m_heapAllocs_.contains(var->alloc.heap->base)) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format(
              "trying to access /Bmemory which has been already freed/R using this heap pointer"),
          .notes = {},
      });

      *m_exit_ = true;
      return 0;
    }

    if ((var->size + offset) >= var->alloc.heap->size) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format("trying to /Baccess memory outside of what has been allocated/R and assigned to this heap pointer"),
          .notes = {
            Diagnostic{
              .severity = DiagnosticSeverity::Note,
              .range = static_cast<SourceMultiRange>(var->alloc.heap->range),
              .message = Diagnostics::format("pointer points to /Bheap memory allocated here/R"),
              .notes = {},
            },
          },
      });

      *m_exit_ = true;
      return 0;
    }

    return var->alloc.heap->base + offset;
  }

  [[nodiscard]] auto access_heap(const IrPointerValue* var, const char* op_name,
                                 SourceRange var_range) -> std::uintptr_t {
    return access_heap(var, var->offset, op_name, var_range);
  }

  [[nodiscard]] auto access_stack(const IrPointerValue* var, SourceRange var_range)
      -> const IrValue* {
    if (var->origin == IrPointerOrigin::Heap) {
      assert(false);
      *m_exit_ = true;
      return nullptr;
    }

    if (var->alloc.stack == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format(
              "trying to access this stack pointer which /Bdoesn't point to any stack variable/R"),
          .notes = {},
      });

      *m_exit_ = true;
      return nullptr;
    }

    IrStackAllocMetadata* alloc = find_stack_alloc(var->alloc.stack);
    if (alloc == nullptr) {
      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format("this stack pointer points to a stack variable /Bthat has "
                                         "been already destroyed/R (access is disallowed)"),
          .notes = {},
      });

      *m_exit_ = true;
      return nullptr;
    }

    return var->alloc.stack;
  }

  void ref_check(IrPointerValue* var, const IrPointerValue* val, SourceRange var_range) {
    if (var->origin != IrPointerOrigin::Heap) {
      assert(false);
      *m_exit_ = true;
      return;
    }

    IrHeapAllocMetadata* var_alloc = var->alloc.heap;
    if (var_alloc == nullptr) {
      assert(false);
      return;
    }

    if (m_heapAllocs_.contains(var_alloc->base)) {
      IrHeapAllocMetadata* val_alloc = val->alloc.heap;
      if (var_alloc != val_alloc || val_alloc == nullptr) {
        if (--var_alloc->refCount == 0) {
          Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(var_range),
          .message = Diagnostics::format("/Boverwriting the last reference to memory/R that has been allocated on the heap, this will lead to a memory leak"),
          .notes = {
            Diagnostic{
              .severity = DiagnosticSeverity::Note,
              .range = static_cast<SourceMultiRange>(var_alloc->range),
              .message = Diagnostics::format("pointer points to /Bheap memory allocated here/R"),
              .notes = {},
            },
          },
      });

          *m_exit_ = true;
        }
        return;
      }

      if (!var->refCounted) {
        ++var_alloc->refCount;
        var->refCounted = true;
      }
    }
  }

 private:
  bool* m_exit_{ nullptr };

  std::unordered_map<std::uintptr_t, IrHeapAllocMetadata*>  m_heapAllocs_;
  std::unordered_map<std::uintptr_t, IrStackAllocMetadata*> m_stackAllocs_;
};

#endif  // JLD_MCC_IR_INTERPRETER_ALLOCATOR_H
