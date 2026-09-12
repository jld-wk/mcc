// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cstddef>
#include <exception>
#include <fstream>
#include <ios>
#include <print>
#include <string_view>
#include <variant>
#include <vector>

#include "decls.h"
#include "ir/interpreter.h"
#include "passes/semantic.h"
#include "syntaxer.h"
#include "tokenize.h"
#include "type_arena.h"
#include "utility.h"

auto main() -> int {
  try {
    std::println("Hello mcc !\n");

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
    std::println("");

    TypeArena types;

    Syntaxer syntaxer{ tokens, types };
    // TODO(jld-wk): not actually an AST yet, have to implement a FileDecl
    std::vector<Decl*> ast = syntaxer.build();

    // need like an ast pretty printer

    for (Decl* decl : ast) {
      std::visit(
          Overload{
              [](const FunctionDecl& decl) -> void {
                std::println("Function Decl: {}", decl.identifer);
              },
          },
          decl->variant);
    }
    std::println("");

    SemanticPass<true> semantic_pass{ types };
    semantic_pass.analyze(ast);

    std::println("");

    IrInterpreter interpreter;
    interpreter.interpret_file("source.ir");

  } catch (const std::exception& e) {
    return 1;
  }

  return 0;
}