// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_SOURCE_H
#define JLD_MCC_SOURCE_H

#include <cassert>
#include <cstddef>
#include <cstdint>

using FileId = size_t;

struct SourceLocation {
  uint32_t line;
  uint32_t column;
};

struct SourceRange {
  FileId file;

  SourceLocation begin;
  SourceLocation end;

  size_t beginIt;
  size_t endIt;
};

auto source_range_from(const SourceRange& start, const SourceRange& end) -> SourceRange {
  assert(start.file == end.file);
  return SourceRange{ .file = start.file,
                      .begin = start.begin,
                      .end = end.end,
                      .beginIt = start.beginIt,
                      .endIt = end.endIt };
}

#endif  // JLD_MCC_SOURCE_H