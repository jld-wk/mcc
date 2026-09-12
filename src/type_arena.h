// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_TYPE_ARENA_H
#define JLD_MCC_TYPE_ARENA_H

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <variant>

#include "arena.h"
#include "types.h"
#include "utility.h"

class TypeArena {
 public:
  TypeArena() {
    m_blocks_ = allocate<Type*>(c_blocksCount);
    allocate_new_slots();
  }

  ~TypeArena() {
    for (size_t b = 0; b < m_blockIdx_; ++b) deallocate(m_blocks_[b]);
    deallocate(m_blocks_);
    assert(m_freed_ == m_allocated_);
  }

  auto query(TypeVariant variant) const -> Type* {
    const auto it = m_typeIds_.find(variant);
    if (it == m_typeIds_.end())
      return nullptr;
    TypeId type_id = it->second;
    return &m_blocks_[type_id.blockIdx][type_id.slotIdx];
  }

  auto emplace(TypeVariant variant) -> Type* {
    if (m_slotIdx_ >= c_slotsCount)
      allocate_new_slots();
    Type* type = query(variant);

    if (type == nullptr) {
      TypeId type_id{ .slotIdx = m_slotIdx_, .blockIdx = m_blockIdx_ - 1 };
      Type*  ptr{ &m_slots_[m_slotIdx_++] };
      std::construct_at(ptr, variant);
      m_typeIds_[variant] = type_id;
      return ptr;
    }

    return type;
  }

 private:
  void allocate_new_slots() {
    // TODO(jld-wk): Fix that... I mean it's a total of 10000 slots but it might not be enough
    assert(m_blockIdx_ < c_blocksCount);

    m_slotIdx_ = 0;
    m_slots_ = allocate<Type>(c_slotsCount);
    m_blocks_[m_blockIdx_++] = m_slots_;
  }

  // TODO(jld-wk): Some debug checks for testing, strip out at release builds
  template <typename Type>
  auto allocate(size_t count) -> Type* {
    const size_t size{ count * sizeof(Type) };
    m_allocated_ += size;
    char* ptr{ static_cast<char*>(malloc(size + sizeof(size_t))) };
    *reinterpret_cast<size_t*>(ptr) = size;
    return reinterpret_cast<Type*>(ptr + sizeof(size_t));
  }

  template <typename Type>
  void deallocate(Type* ptr) {
    char*      c_ptr{ reinterpret_cast<char*>(ptr) - sizeof(size_t) };
    const auto size{ *reinterpret_cast<size_t*>(c_ptr) };
    m_freed_ += size;
    ptr->~Type();
    free(c_ptr);
  }

 private:
  size_t m_freed_{ 0 };
  size_t m_allocated_{ 0 };

  Type*  m_slots_{ nullptr };
  size_t m_slotIdx_{ 0 };

  Type** m_blocks_{ nullptr };
  size_t m_blockIdx_{ 0 };

  struct TypeId {
    size_t slotIdx{ 0 };
    size_t blockIdx{ 0 };
  };

  struct TypeVariantHasher {
    auto operator()(TypeVariant variant) const -> size_t {
      return std::visit(
          Overload{
              [=](BuiltinType type) -> size_t {
                return variant.index() ^ (static_cast<size_t>(type.kind) << 1);
              },
          },
          variant);
    };
  };

  std::unordered_map<TypeVariant, TypeId, TypeVariantHasher> m_typeIds_;
};

#endif  // JLD_MCC_TYPE_ARENA_H