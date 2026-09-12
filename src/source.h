// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_SOURCE_H
#define JLD_MCC_SOURCE_H

#include <cstddef>
#include <cstdint>

struct SourceRange {
  // TODO(jld-wk): i don't think these are actually needed, maybe later for expressions, but have to
  // see
  size_t start;
  size_t end;

  uint32_t startLine;
  uint32_t startColumn;

  uint32_t endLine;
  uint32_t endColumn;
};

auto source_range_from(const SourceRange& start, const SourceRange& end) -> SourceRange {
  return SourceRange{
    .start = start.start,
    .end = end.end,
    .startLine = start.startLine,
    .startColumn = start.startColumn,
    .endLine = end.endLine,
    .endColumn = end.endColumn,
  };
}

#endif  // JLD_MCC_SOURCE_H