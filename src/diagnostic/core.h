// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_DIAGNOSTIC_CORE_H
#define JLD_MCC_DIAGNOSTIC_CORE_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <print>
#include <regex>
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
  SourceMultiRange   range;
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
    for (const Diagnostic& diagnostic : s_diagnostics_) print_diagnostic(diagnostic);
  }

  static void clear() {
    s_diagnostics_.clear();
  }

  template <typename... Args>
  static constexpr auto format(std::format_string<Args...> fmt, Args&&... args) -> std::string {
    std::string formatted = std::format<Args...>(fmt, std::forward<Args>(args)...);
    formatted = std::regex_replace(formatted, std::regex("/B"), "\033[1m");
    formatted = std::regex_replace(formatted, std::regex("/R"), "\033[m");
    return formatted;
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
    if (s_sourceManager_ == nullptr)
      return;
    // TODO(jld-wk): Fix hardcoded 1
    const SourceFile& file{ s_sourceManager_->find(2) };
    print_diagnostic(diagnostic, file);

    for (const Diagnostic& note : diagnostic.notes) print_diagnostic(note);
  }

  static void print_diagnostic(const Diagnostic& diagnostic, const SourceFile& file) {
    const std::string_view source{ file.source };
    if (source.empty())
      return;

    std::println(stderr, "\033[1m\033[41;255m{}\033[m: {}", format_severity(diagnostic.severity),
                 diagnostic.message);
    if (diagnostic.range.begin.line == 0) {
      std::println();
      return;
    }

    uint32_t         line{ 0 };
    size_t           start{ 0 };
    std::string_view tgt_source_line;

    while (start < source.size()) {
      size_t end{ source.find('\n', start) };
      if (end == std::string_view::npos)
        end = source.size() - 1;

      ++line;
      if (line == diagnostic.range.begin.line) {
        tgt_source_line = source.substr(start, end - start);
        break;
      }
      start = end + 1;
    }

    if (tgt_source_line.empty())
      return;

    const std::string_view bold_code{ "\033[1m\033[31m" };
    const std::string_view reset_code{ "\033[m" };

    const size_t source_line_size =
        tgt_source_line.size() + bold_code.size() + reset_code.size() + 1;
    char* source_line = new char[source_line_size];
    source_line[source_line_size - 1] = '\0';

    const uint32_t tgt_column{ diagnostic.range.begin.column - 1 };
    const uint32_t tgt_length{ diagnostic.range.end.column - diagnostic.range.begin.column };

    size_t offset = 0;
    memcpy(source_line, tgt_source_line.data(), tgt_column);
    offset += tgt_column;
    memcpy(source_line + offset, bold_code.data(), bold_code.size());
    offset += bold_code.size();
    memcpy(source_line + offset, tgt_source_line.data() + tgt_column, tgt_length);
    offset += tgt_length;
    memcpy(source_line + offset, reset_code.data(), reset_code.size());
    offset += reset_code.size();
    memcpy(source_line + offset, tgt_source_line.data() + tgt_column + tgt_length,
           tgt_source_line.size() - tgt_column - tgt_length);

    std::string indicator(tgt_column + tgt_length, ' ');
    indicator[tgt_column] = '^';
    for (uint32_t i = 1; i < tgt_length; ++i) indicator[i + tgt_column] = '~';

    std::string line_number{ std::to_string(diagnostic.range.begin.line) };
    std::string line_spaces(5 - line_number.size(), ' ');

    std::println(stderr, "\033[1m\033[255m ----> {}:{}:{}\033[m", file.path,
                 diagnostic.range.begin.line, tgt_column + 1);
    std::println(stderr, "\033[1m\033[255m {}{} |\033[m {}", line_spaces, line_number, source_line);
    std::println(stderr, "       \033[1m|\033[1m\033[31m {}\033[m", indicator);

    delete[] source_line;
  }

 private:
  static inline std::vector<Diagnostic> s_diagnostics_;
  static inline SourceManager*          s_sourceManager_{ nullptr };
};

#endif  // JLD_MCC_DIAGNOSTIC_CORE_H