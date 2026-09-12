// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cstddef>
#include <exception>
#include <fstream>
#include <ios>
#include <print>
#include <string_view>
#include <vector>

#include "arenas.h"
#include "tokenize.h"

class C {
 public:
  C()
      : m_a_(0)
      , m_b_(0) {}
  C(int a, int b)
      : m_a_(a)
      , m_b_(b) {}

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
  try {
    std::println("Hello mcc !");

    std::fstream file{ "source.c" };
    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<char> buf(size + 1);
    buf[size] = '\0';

    file.read(buf.data(), static_cast<std::streamsize>(size));

    std::string_view source{ buf.data(), size + 1 };

    Tokenizer                 tokenizer;
    const std::vector<Token>& tokens = tokenizer.tokenize(source);

    for (const Token& token : tokens)
      std::println("{} {} -> line(s/e): {}/{} | col(s/e): {}/{}", format_token_kind(token.kind),
                   token.text, token.source.startLine, token.source.endLine,
                   token.source.startColumn, token.source.endColumn);

    Arena<C> c;
    c.emplace(10, 10);
  } catch (const std::exception& e) {
    return 1;
  }

  return 0;
}