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

#include "syntaxer.h"
#include "tokenize.h"

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

    Syntaxer syntaxer{ tokens };
    syntaxer.build();
  } catch (const std::exception& e) {
    return 1;
  }

  return 0;
}