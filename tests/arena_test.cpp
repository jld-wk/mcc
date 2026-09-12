// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cstddef>

#include "arenas.h"

class C {
 public:
  C()
      : m_a_(0)
      , m_b_(0) {
    assert(false);
  }
  C(int a, int b)
      : m_a_(a)
      , m_b_(b) {
    assert((a % 10 == 1) && (b % 10 == 1));
  }

  [[nodiscard]] auto a() const -> int {
    return m_a_;
  }

  [[nodiscard]] auto b() const -> int {
    return m_b_;
  }

 private:
  int m_a_;
  int m_b_;
};

auto main() -> int {
  Arena<C> c_arena;

  C* c_0 = c_arena.emplace(11, 11);
  assert(c_0->a() == 11 && c_0->b() == 11);

  C* c_1 = c_arena.emplace(101, 101);
  assert(c_1->a() == 101 && c_1->b() == 101);

  for (size_t i = 0; i < 98; ++i) {
    C* c = c_arena.emplace(11, 11);
    assert(c->a() == 11 && c->b() == 11);
  }

  C* c_2 = c_arena.emplace(1001, 1001);
  assert(c_2->a() == 1001 && c_2->b() == 1001);

  C* c_3 = c_arena.emplace(1000001, 1000001);

  for (size_t i = 0; i < 1000; ++i) c_arena.emplace(11, 11);

  assert(c_3->a() == 1000001);
  assert(c_3->b() == 1000001);

  return 0;
}