// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_ARENAS
#define JLD_MCC_ARENAS

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

static inline constexpr size_t c_slotsCount = 100;
static inline constexpr size_t c_blocksCount = 100;

template <typename SlotType>
class Arena {
  static_assert(std::is_trivially_destructible_v<SlotType>);

 public:
  Arena() {
    m_blocks_ = allocate<SlotType*>(c_blocksCount);
    allocate_new_slots();
  }

  ~Arena() {
    for (size_t b = 0; b < m_blockIdx_; ++b) deallocate(m_blocks_[b]);
    deallocate(m_blocks_);
    assert(m_freed_ == m_allocated_);
  }

  template <typename... Args>
  auto emplace(Args&&... args) -> SlotType* {
    if (m_slotIdx_ >= c_slotsCount)
      allocate_new_slots();
    SlotType* ptr = &m_slots_[m_slotIdx_++];
    std::construct_at(ptr, std::forward<Args>(args)...);
    return ptr;
  }

 private:
  void allocate_new_slots() {
    // TODO(jld-wk): Fix that... I mean it's a total of 10000 slots but it might not be enough
    assert(m_blockIdx_ < c_blocksCount);

    m_slotIdx_ = 0;
    m_slots_ = allocate<SlotType>(c_slotsCount);
    m_blocks_[m_blockIdx_++] = m_slots_;
  }

  // TODO(jld-wk): Some debug checks for testing, strip out at release builds
  template <typename Type>
  auto allocate(size_t count) -> Type* {
    const size_t size = count * sizeof(Type);
    m_allocated_ += size;
    char* ptr = static_cast<char*>(malloc(size + sizeof(size_t)));
    *reinterpret_cast<size_t*>(ptr) = size;
    return reinterpret_cast<Type*>(ptr + sizeof(size_t));
  }

  template <typename Type>
  void deallocate(Type ptr) {
    char*      c_ptr = reinterpret_cast<char*>(ptr) - sizeof(size_t);
    const auto size = *reinterpret_cast<size_t*>(c_ptr);
    m_freed_ += size;
    free(c_ptr);
  }

 private:
  size_t m_freed_ = 0;
  size_t m_allocated_ = 0;

  SlotType* m_slots_{ nullptr };
  size_t    m_slotIdx_{ 0 };

  SlotType** m_blocks_{ nullptr };
  size_t     m_blockIdx_{ 0 };
};

#endif