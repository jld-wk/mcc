// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <exception>
#include <print>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "decls.h"
#include "diagnostic/core.h"
#include "diagnostic/source_manager.h"
#include "ir/interpreter/core.h"
#include "passes/semantic.h"
#include "syntaxer.h"
#include "tokenizer.h"
#include "type_arena.h"
#include "utility.h"

auto main() -> int {
  try {
    std::println("Hello mcc !\n");

    SourceManager source_manager;
    // TODO(jld-wk): NO!
    Diagnostics::init(&source_manager);

    FileId           file{ source_manager.open("source.c") };
    std::string_view source{ source_manager.find(file).source };

    Tokenizer                 tokenizer;
    const std::vector<Token>& tokens = tokenizer.tokenize(file, source);

    for (const Token& token : tokens)
      std::println("{} -> {}", format_token_kind(token.kind), token.text);
    std::println("");

    TypeArena types;

    Syntaxer syntaxer{ tokens, types };
    // TODO(jld-wk): not actually an AST yet, have to implement a FileDecl
    std::vector<Decl*> ast{ syntaxer.build() };

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

    IrInterpreter interpreter{ types };
    interpreter.interpret_file("source.mir", source_manager);

  } catch (const std::exception& e) {
    return 1;
  }

  return 0;
}