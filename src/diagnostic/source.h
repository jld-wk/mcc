// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_SOURCE_H
#define JLD_MCC_SOURCE_H

#include <cassert>
#include <cstdint>

struct SourceLocation {
  uint32_t line{ 0 };
  uint32_t column{ 0 };
};

struct SourceMultiRange {
  SourceLocation begin;
  SourceLocation end;
};

struct SourceRange {
  uint32_t line{ 0 };
  uint32_t column{ 0 };
  uint32_t length{ 0 };

  explicit operator SourceMultiRange() const {
    return SourceMultiRange{
      .begin = SourceLocation{ .line = line, .column = column },
      .end = SourceLocation{ .line = line, .column = column + length },
    };
  }
};

auto source_multi_range_from(const SourceRange& start, const SourceRange& end) -> SourceMultiRange {
  return SourceMultiRange{
    .begin = SourceLocation{ .line = start.line, .column = start.column },
    .end = SourceLocation{ .line = end.line, .column = end.column + end.length },
  };
}

#endif  // JLD_MCC_SOURCE_H