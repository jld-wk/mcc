// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <ios>
#include <print>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

template <typename... Args>
constexpr auto format_styled(std::format_string<Args...> fmt, Args&&... args) -> std::string {
  std::string formatted = std::format<Args...>(fmt, std::forward<Args>(args)...);
  formatted = std::regex_replace(formatted, std::regex("/B"), "\033[1m");
  formatted = std::regex_replace(formatted, std::regex("/R"), "\033[m");
  return formatted;
}

auto main() -> int {
  const uint32_t tgt_line = 39;
  const uint32_t tgt_column = 15;
  const uint32_t tgt_length = 4;
  const char*    tgt_filepath = "source.mir";

  std::fstream stream{ tgt_filepath };
  stream.seekg(0, std::ios::end);
  size_t size{ static_cast<size_t>(stream.tellg()) };
  stream.seekg(0, std::ios::beg);

  char* source_buf = new char[size];
  stream.read(source_buf, static_cast<std::streamsize>(size));

  const std::string_view source{ source_buf, size };

  uint32_t         line{ 0 };
  size_t           start{ 0 };
  std::string_view tgt_source_line;

  while (start < source.size()) {
    size_t end{ source.find('\n', start) };
    if (end == std::string_view::npos)
      end = source.size() - 1;

    ++line;
    if (line == tgt_line) {
      tgt_source_line = source.substr(start, end);
      break;
    }
    start = end + 1;
  }

  assert(!tgt_source_line.empty());

  const std::string_view bold_code{ "\033[1m\033[31m" };
  const std::string_view reset_code{ "\033[m" };

  const size_t source_line_size = tgt_source_line.size() + bold_code.size() + reset_code.size();
  char*        source_line = new char[source_line_size];
  source_line[source_line_size - 1] = '\0';

  memcpy(source_line, tgt_source_line.data(), tgt_column);
  memcpy(source_line + tgt_column, bold_code.data(), bold_code.size());
  memcpy(source_line + tgt_column + bold_code.size(), tgt_source_line.data() + tgt_column,
         tgt_length);
  memcpy(source_line + tgt_column + bold_code.size() + tgt_length, reset_code.data(),
         reset_code.size());

  std::string indicator(tgt_column + tgt_length, ' ');
  indicator[tgt_column] = '^';
  for (uint32_t i = 1; i < tgt_length; ++i) indicator[i + tgt_column] = '~';

  std::println(
      stderr, "\033[1m\033[41;255m{}\033[m: {}", "<error>",
      format_styled("/B'{}'/R: variable not declared in label /B'{}'/R in function /B'{}'/R", "c",
                    "cond", "print_char_ptr"));

  std::string line_number{ std::to_string(tgt_line) };
  std::string line_spaces(5 - line_number.size(), ' ');

  std::println(stderr, "\033[1m\033[255m ----> {}:{}:{}\033[m", tgt_filepath, tgt_line,
               tgt_column + 1);
  std::println(stderr, "\033[1m\033[255m {}{} |\033[m {}", line_spaces, line_number, source_line);
  std::println(stderr, "       \033[1m|\033[1m\033[31m {}\033[m", indicator);

  delete[] source_line;
  delete[] source_buf;
}