// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_DIAGNOSTIC_CORE_H
#define JLD_MCC_DIAGNOSTIC_CORE_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "source.h"
#include "source_manager.h"

enum class DiagnosticSeverity : uint8_t {
  Note,
  Warning,
  Error,
  Fatal,
};

struct Diagnostic {
  DiagnosticSeverity severity;
  SourceRange        range;
  std::string        message;

  std::vector<Diagnostic> notes;
};

class Diagnostics {
 public:
  static void init(SourceManager* source_manager) {
    s_sourceManager_ = source_manager;
  }

  static void report(Diagnostic diagnostic) {
    s_diagnostics_.push_back(std::move(diagnostic));
  }

  static auto error_count() -> uint32_t {
    uint32_t count{ 0 };
    for (const Diagnostic& diagnostic : s_diagnostics_) {
      if (diagnostic.severity == DiagnosticSeverity::Error ||
          diagnostic.severity == DiagnosticSeverity::Fatal)
        ++count;
    }
    return count;
  }

  static void print() {
    for (const Diagnostic& diagnostic : s_diagnostics_) {
      print_diagnostic(diagnostic);
      std::println(stderr, "");
    }
  }

  static void clear() {
    s_diagnostics_.clear();
  }

 private:
  static auto format_severity(DiagnosticSeverity severity) -> const char* {
    switch (severity) {
      case DiagnosticSeverity::Note:
        return "<note>";
      case DiagnosticSeverity::Warning:
        return "<warning>";
      case DiagnosticSeverity::Error:
        return "<error>";
      case DiagnosticSeverity::Fatal:
        return "<fatal>";
    }
    return "<unknown>";
  }

  static void print_diagnostic(const Diagnostic& diagnostic) {
    std::println(stderr, "{}: {}", format_severity(diagnostic.severity), diagnostic.message);

    if (s_sourceManager_ == nullptr)
      return;
    const SourceFile& file{ s_sourceManager_->query(diagnostic.range.file) };

    std::println(stderr, " --> {}:{}:{}", file.path, diagnostic.range.begin.line,
                 diagnostic.range.begin.column);
    print_source(diagnostic, file);

    for (const Diagnostic& note : diagnostic.notes) {
      std::println(stderr);
      print_note(note);
    }
  }

  static void print_note(const Diagnostic& diagnostic) {
    std::println(stderr, "note: {}", diagnostic.message);

    if (s_sourceManager_ == nullptr)
      return;
    const SourceFile& file{ s_sourceManager_->query(diagnostic.range.file) };

    std::println(stderr, " --> {}:{}:{}", file.path, diagnostic.range.begin.line,
                 diagnostic.range.begin.column);
    print_source(diagnostic, file);
  }

  static void print_source(const Diagnostic& diagnostic, const SourceFile& file) {
    const std::span<char> source{ file.source };
    if (source.empty())
      return;

    const std::string_view diagnostic_source{ source.data() + diagnostic.range.beginIt,
                                              diagnostic.range.endIt - diagnostic.range.beginIt };

    const uint32_t line_number{ diagnostic.range.begin.line };
    const size_t   line_digits{ std::to_string(line_number).size() };

    std::println(stderr, " {} |", std::string(line_digits, ' '));
    std::println(stderr, " {} | {}", line_number, diagnostic_source);
    std::print(stderr, " {} | ", std::string(line_digits, ' '));

    const uint32_t column{ diagnostic.range.begin.column > 0 ? diagnostic.range.begin.column - 1
                                                             : 0 };
    const uint32_t clamped_column{ std::min(column,
                                            static_cast<uint32_t>(diagnostic_source.size())) };

    std::print(stderr, "{}^", std::string(clamped_column, ' '));

    if (diagnostic.range.end.line == diagnostic.range.begin.line &&
        diagnostic.range.end.column > diagnostic.range.begin.column) {
      const size_t length{ diagnostic.range.end.column - diagnostic.range.begin.column };
      if (length > 1) {
        const size_t available{ diagnostic_source.size() - clamped_column };
        const size_t highlight{ std::min(length - 1, available) };
        std::println(stderr, "{}", std::string(highlight, '~'));
      }
    }

    std::println(stderr, " {} |", std::string(line_digits, ' '));
  }

 private:
  static inline std::vector<Diagnostic> s_diagnostics_;
  static inline SourceManager*          s_sourceManager_{ nullptr };
};

#endif  // JLD_MCC_DIAGNOSTIC_CORE_H