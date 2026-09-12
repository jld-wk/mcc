// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_UTILITY
#define JLD_MCC_UTILITY

// TODO(jld-wk): portability
#define JLD_MCC_FORCE_INLINE [[gnu::always_inline]]

template <class... Ts>
struct Overload : Ts... {
  using Ts::operator()...;
};

#endif  // JLD_MCC_UTILITY_H