// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_OVERLOADER_H
#define JLD_MCC_IR_OVERLOADER_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <variant>

#include "ir/insts.h"
#include "ir/values.h"
#include "types.h"

class IrOverloader {
 public:
  using ArithmeticFunc = std::function<IrValue(const IrValue* a, const IrValue* b)>;

 public:
  explicit IrOverloader(bool* exit)
      : m_exit_{ exit } {}

  auto arithmetic_op(IrArithmeticOpKind op_kind, const IrValue* arg0, const IrValue* arg1)
      -> IrValue {
    Type* arg0_type{ arg0->type };
    Type* arg1_type{ arg1->type };

    /*
    struct TryPtrArithmeticRes {
      bool    success{ false };
      IrValue val;
    };

    auto try_ptr_arithmetic = [](IrArithmeticOpKind kind, const IrValue* a, const IrValue* b,
                                 Type* a_type, Type* b_type) -> TryPtrArithmeticRes {
      if (size_t offset{ a_type->is_ptr() }; offset != 0 && b_type->is_integer() &&
                                             kind != IrArithmeticOpKind::Mul &&
                                             kind != IrArithmeticOpKind::Div) {
        const IrValuePointer* a_ptr{ std::get_if<IrValuePointer>(&a->data) };
        const IrValueInteger* b_int{ std::get_if<IrValueInteger>(&b->data) };

        auto res{ b_int->convert_implicitly<size_t>() };
        if (!res.positive) {
          return TryPtrArithmeticRes{ .success = true, .val = make_ir_error_value() };
        }

        return TryPtrArithmeticRes{
          .success = true,
          .val =
              IrValue{
                  .data =
                      IrValuePointer{
                          .addr = kind == IrArithmeticOpKind::Add
                                      ? a_ptr->addr + static_cast<std::uintptr_t>(res.converted)
                                      : a_ptr->addr - static_cast<std::uintptr_t>(res.converted),
                      },
                  .type = a->type,
              },
        };
      }

      return TryPtrArithmeticRes{
        .success = false,
        .val = make_ir_error_value(),
      };
    };

    TryPtrArithmeticRes res_0{ try_ptr_arithmetic(kind, a, b, a_type, b_type) };
    if (res_0.success)
      return res_0.val;
    TryPtrArithmeticRes res_1{ try_ptr_arithmetic(kind, b, a, b_type, a_type) };
    if (res_1.success)
      return res_1.val;
    */
    const IrIntegerValue* arg0_int{ std::get_if<IrIntegerValue>(&arg0->data) };
    const IrIntegerValue* arg1_int{ std::get_if<IrIntegerValue>(&arg1->data) };

    uint8_t       p_bits{ std::max(arg0_int->bits, arg1_int->bits) };
    Type*         p_type{ arg0_int->bits == p_bits ? arg0_type : arg1_type };
    IrIntegerType p_int_type{ arg0_int->bits == p_bits ? arg0_int->type : arg1_int->type };

    switch (op_kind) {
      case IrArithmeticOpKind::Add:
        if (arg0_int->add_overflow(p_int_type, arg1_int->val))
          break;
        return IrValue{ .data = IrIntegerValue{ arg0_int->val + arg1_int->val, p_bits, p_int_type },
                        .type = p_type };
      case IrArithmeticOpKind::Sub:
        if (arg0_int->sub_overflow(p_int_type, arg1_int->val))
          break;
        return IrValue{ .data = IrIntegerValue{ arg0_int->val - arg1_int->val, p_bits, p_int_type },
                        .type = p_type };
      case IrArithmeticOpKind::Mul:
        if (arg0_int->mul_overflow(p_int_type, arg1_int->val))
          break;
        return IrValue{ .data = IrIntegerValue{ arg0_int->val * arg1_int->val, p_bits, p_int_type },
                        .type = p_type };
      case IrArithmeticOpKind::Div:
        // only need a division overflow check for floating point numbers? I think, I hope
        return IrValue{ .data = IrIntegerValue{ arg0_int->val / arg1_int->val, p_bits, p_int_type },
                        .type = p_type };
    }

    // TODO(jld-wk): handle overflow

    *m_exit_ = true;

    assert(false);
    return make_ir_error_value();
  }

  auto comparision_op(IrComparisionOpKind op_kind, const IrValue* arg0, const IrValue* arg1)
      -> bool {
    const IrIntegerValue* arg0_int{ std::get_if<IrIntegerValue>(&arg0->data) };
    const IrIntegerValue* arg1_int{ std::get_if<IrIntegerValue>(&arg1->data) };

    // currently theres no concept of signed integer types so theres no need to check for overflows
    // or special promotion
    // uint8_t       p_bits{ std::max(arg0_int->bits, arg1_int->bits) };
    // Type* p_type{ arg0_int->bits == p_bits ? arg0_type : arg1_type };
    // IrIntegerType p_int_type{ arg0_int->bits == p_bits ? arg0_int->type : arg1_int->type };

    return op_kind == IrComparisionOpKind::Eq   ? (arg0_int->val == arg1_int->val)
           : op_kind == IrComparisionOpKind::Ne ? (arg0_int->val != arg1_int->val)
           : op_kind == IrComparisionOpKind::Lt ? (arg0_int->val < arg1_int->val)
           : op_kind == IrComparisionOpKind::Le ? (arg0_int->val <= arg1_int->val)
           : op_kind == IrComparisionOpKind::Gt ? (arg0_int->val > arg1_int->val)
           : op_kind == IrComparisionOpKind::Ge ? (arg0_int->val >= arg1_int->val)
                                                : false;
  }

 private:
  void insert_arithmetic(Type* a, Type* b, IrArithmeticOpKind kind, ArithmeticFunc&& func) {
    m_arithmeticOverloads_.try_emplace(
        ArithmeticKey{
            .a = a,
            .b = b,
            .kind = kind,
        },
        func);
  }

 private:
  bool* m_exit_{ nullptr };

  struct ArithmeticKey {
    Type*              a;
    Type*              b;
    IrArithmeticOpKind kind;

    auto operator==(const ArithmeticKey& other) const -> bool {
      return a == other.a && b == other.b && kind == other.kind;
    }
  };

  struct ArithmeticKeyHasher {
    auto operator()(const ArithmeticKey& key) const noexcept -> size_t {
      size_t h = std::hash<std::uintptr_t>{}(reinterpret_cast<std::uintptr_t>(key.a));
      h ^= (h << 16) ^ 0xFFFF;
      h ^= std::hash<std::uintptr_t>{}(reinterpret_cast<std::uintptr_t>(key.b)) ^ (h >> 5);
      h ^= static_cast<size_t>(key.kind);
      return h;
    }
  };

  std::unordered_map<ArithmeticKey, ArithmeticFunc, ArithmeticKeyHasher> m_arithmeticOverloads_;
};

#endif  // JLD_MCC_IR_OVERLOADER_H
