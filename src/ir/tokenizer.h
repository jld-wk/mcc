// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_TOKENIZER_H
#define JLD_MCC_IR_TOKENIZER_H

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <string_view>
#include <vector>

#include "diagnostic/core.h"
#include "diagnostic/source.h"
#include "diagnostic/source_manager.h"

enum class IrTokenKind : uint8_t {
  Char,
  Slot,
  Number,
  Identifier,

  KywLoad,
  KywStore,

  KywAdd,
  KywSub,
  KywMul,
  KywDiv,

  KywEq,
  KywNe,
  KywLt,
  KywLe,
  KywGt,
  KywGe,

  KywJmp,
  KywCall,
  KywJmpIf,
  KywCallIf,

  KywAlloc,
  KywFree,
  KywLoadAddr,
  KywStoreAt,
  KywStoreAddr,

  KywArg,
  KywRet,
  KywEnd,

  KywExit,
  KywPrint,

  KywU8,
  KywVoid,
  KywU32,

  Dot,
  Star,
  Colon,
  Comma,
  Minus,
  Arrow,

  OpenParen,
  CloseParen,

  Unknown,
  EndOfFile
};

struct IrToken {
  std::string_view text;
  SourceRange      range;
  IrTokenKind      kind;
};

class IrTokenizer {
 public:
  auto tokenize(FileId file, std::string_view stream) -> const std::vector<IrToken>& {
    m_file_ = file;
    m_source_ = stream;
    m_sourceIt_ = stream.begin();

    for (char c{ *m_sourceIt_ }; m_sourceIt_ != stream.end(); c = *m_sourceIt_) {
      m_startLine_ = m_line_;
      m_startColumn_ = m_column_;
      m_startIterated_ = m_iterated_;

      iterate();

      if (c == '\0') {
        push_token(IrTokenKind::EndOfFile);
        return m_tokens_;
      }

      if (c == '\n') {
        ++m_line_;
        m_column_ = 1;
        continue;
      }

      if (c == ';') {
        if (*m_sourceIt_ == ';') {
          iterate();

          while (*m_sourceIt_ != ';') {
            if (*m_sourceIt_ == '\n')
              ++m_line_;
            if (*m_sourceIt_ == '\0') {
              push_token(IrTokenKind::EndOfFile);
              return m_tokens_;
            }
            iterate();
          }

          iterate();

          if (*m_sourceIt_ != ';') {
            Diagnostics::report(Diagnostic{
                .severity = DiagnosticSeverity::Error,
                .range = static_cast<SourceMultiRange>(SourceRange{
                    .line = m_line_,
                    .column = m_column_,
                    .length = 2,
                }),
                .message = "Expected ';;' (double semicolon) to close the multi-line comment",
                .notes = { Diagnostic{
                    .severity = DiagnosticSeverity::Note,
                    .range = static_cast<SourceMultiRange>(SourceRange{
                        .line = m_startLine_,
                        .column = m_startColumn_,
                        .length = 2,
                    }),
                    .message = "Multi-line comment started here",
                    .notes = {},
                } },
            });
          }

          continue;
        }

        while (*m_sourceIt_ != '\n') iterate();
        continue;
      }

      if (std::isspace(c))
        continue;

      if (std::isalpha(c)) {
        while (is_identifier(*m_sourceIt_)) iterate();

        std::string_view view{ str_view() };
        IrTokenKind      kind{ keyword_kind(view) };

        if (kind != IrTokenKind::Unknown)
          push_token_str_view(kind, view);
        else
          push_token_str_view(IrTokenKind::Identifier, view);
        continue;
      }

      if (std::isdigit(c)) {
        while (is_identifier(*m_sourceIt_)) iterate();
        push_token(IrTokenKind::Number);
        continue;
      }

      if (c == ':') {
        push_token(IrTokenKind::Colon);
        continue;
      }

      if (c == ',') {
        push_token(IrTokenKind::Comma);
        continue;
      }

      if (c == '.') {
        push_token(IrTokenKind::Dot);
        continue;
      }

      if (c == '*') {
        push_token(IrTokenKind::Star);
        continue;
      }

      if (c == '-') {
        if (*m_sourceIt_ == '>') {
          iterate();
          push_token(IrTokenKind::Arrow);
          continue;
        }

        push_token(IrTokenKind::Minus);
        continue;
      }

      if (c == '(') {
        push_token(IrTokenKind::OpenParen);
        continue;
      }

      if (c == ')') {
        push_token(IrTokenKind::CloseParen);
        continue;
      }

      if (c == '\'') {
        iterate();
        ++m_startIterated_;
        std::string_view literal = str_view();

        switch (*m_sourceIt_) {
          case 'n':
            literal = std::string_view("\n");
            iterate();
            break;
          case 'r':
            literal = std::string_view("\r");
            iterate();
            break;
          case 't':
            literal = std::string_view("\t");
            iterate();
            break;
          case '\\':
            literal = std::string_view("\\");
            iterate();
            break;
          default:
            break;
        }

        if (*m_sourceIt_ != '\'') {
          Diagnostics::report(Diagnostic{
              .severity = DiagnosticSeverity::Error,
              .range = static_cast<SourceMultiRange>(SourceRange{
                  .line = m_line_,
                  .column = m_column_,
                  .length = 1,
              }),
              .message = "Expected an ending ' (single quote) to close the char literal",
              .notes = {},
          });
        }

        iterate();
        push_char_token(IrTokenKind::Char, literal);
        continue;
      }

      // TODO(jld-wk): store 0, [r:rax], so you can then basically bypass
      // the register allocation and also then for globals, store 0, [g:my_literal]
      if (c == '[') {
        while (is_identifier(*m_sourceIt_)) iterate();

        if (*m_sourceIt_ == ']') {
          ++m_startColumn_;
          ++m_startIterated_;
          push_token(IrTokenKind::Slot);
          iterate();
          continue;
        }

        if (*m_sourceIt_ != ':') {
          Diagnostics::report(Diagnostic{
              .severity = DiagnosticSeverity::Error,
              .range = static_cast<SourceMultiRange>(SourceRange{
                  .line = m_line_,
                  .column = m_column_,
                  .length = 1,
              }),
              .message = "Expected a ':' (colon) to seperate the type from the slot index",
              .notes = {},
          });
        }

        iterate();

        while (is_identifier(*m_sourceIt_)) iterate();
        if (*m_sourceIt_ != ']') {
          Diagnostics::report(Diagnostic{
              .severity = DiagnosticSeverity::Error,
              .range = static_cast<SourceMultiRange>(SourceRange{
                  .line = m_line_,
                  .column = m_column_,
                  .length = 1,
              }),
              .message = "Expected a ']' (closing bracket) to close the slot",
              .notes = {},
          });
        }

        ++m_startColumn_;
        ++m_startIterated_;
        push_token(IrTokenKind::Slot);
        iterate();
        continue;
      }

      Diagnostics::report(Diagnostic{
          .severity = DiagnosticSeverity::Error,
          .range = static_cast<SourceMultiRange>(SourceRange{
              .line = m_line_,
              .column = m_column_,
              .length = 1,
          }),
          .message = std::format("Unexpected '{}' -> unable to lex it", c),
          .notes = {},
      });
    }

    // TODO(jld-wk): compiler panic? -> missing \0 terminator
    return m_tokens_;
  }

 private:
  void iterate(uint32_t amount = 1) {
    m_column_ += amount;
    m_iterated_ += amount;
    m_sourceIt_ += amount;
  }

  auto is_identifier(char c) -> bool {
    return std::isalnum(c) || c == '_';
  }

  auto str_view() -> std::string_view {
    size_t size = m_iterated_ - m_startIterated_;
    return std::string_view{ m_source_.data() + m_startIterated_, size };
  }

  void push_char_token(IrTokenKind kind, std::string_view literal) {
    uint32_t length = m_column_ - m_startColumn_;
    m_tokens_.push_back(IrToken{
        .text = literal,
        .range =
            SourceRange{
                .line = m_startLine_,
                .column = m_startColumn_,
                .length = length,
            },
        .kind = kind,
    });
  }

  void push_token(IrTokenKind kind) {
    uint32_t length = m_column_ - m_startColumn_;
    m_tokens_.push_back(IrToken{
        .text = str_view(),
        .range =
            SourceRange{
                .line = m_startLine_,
                .column = m_startColumn_,
                .length = length,
            },
        .kind = kind,
    });
  }

  void push_token_str_view(IrTokenKind kind, std::string_view view) {
    uint32_t length = m_column_ - m_startColumn_;
    m_tokens_.push_back(IrToken{
        .text = view,
        .range =
            SourceRange{
                .line = m_startLine_,
                .column = m_startColumn_,
                .length = length,
            },
        .kind = kind,
    });
  }

  auto keyword_kind(std::string_view view) -> IrTokenKind {
    if (view == "load")
      return IrTokenKind::KywLoad;
    if (view == "store")
      return IrTokenKind::KywStore;

    if (view == "add")
      return IrTokenKind::KywAdd;
    if (view == "sub")
      return IrTokenKind::KywSub;
    if (view == "mul")
      return IrTokenKind::KywMul;
    if (view == "div")
      return IrTokenKind::KywDiv;

    if (view == "eq")
      return IrTokenKind::KywEq;
    if (view == "ne")
      return IrTokenKind::KywNe;
    if (view == "lt")
      return IrTokenKind::KywLt;
    if (view == "le")
      return IrTokenKind::KywLe;
    if (view == "gt")
      return IrTokenKind::KywGt;
    if (view == "ge")
      return IrTokenKind::KywGe;

    if (view == "jmp")
      return IrTokenKind::KywJmp;
    if (view == "call")
      return IrTokenKind::KywCall;
    if (view == "jmp_if")
      return IrTokenKind::KywJmpIf;
    if (view == "call_if")
      return IrTokenKind::KywCallIf;

    if (view == "alloc")
      return IrTokenKind::KywAlloc;
    if (view == "free")
      return IrTokenKind::KywFree;
    if (view == "load_addr")
      return IrTokenKind::KywLoadAddr;
    if (view == "store_addr")
      return IrTokenKind::KywStoreAddr;
    if (view == "store_at")
      return IrTokenKind::KywStoreAt;

    if (view == "exit")
      return IrTokenKind::KywExit;
    if (view == "print")
      return IrTokenKind::KywPrint;

    if (view == "end")
      return IrTokenKind::KywEnd;
    if (view == "ret")
      return IrTokenKind::KywRet;
    if (view == "arg")
      return IrTokenKind::KywArg;

    if (view == "u8")
      return IrTokenKind::KywU8;
    if (view == "u32")
      return IrTokenKind::KywU32;
    if (view == "void")
      return IrTokenKind::KywVoid;

    return IrTokenKind::Unknown;
  }

 private:
  size_t   m_file_{ 0 };
  uint32_t m_line_{ 1 };
  uint32_t m_column_{ 1 };

  uint32_t m_startLine_{ 1 };
  uint32_t m_startColumn_{ 1 };

  size_t m_iterated_{ 0 };
  size_t m_startIterated_{ 0 };

  std::string_view                 m_source_;
  std::string_view::const_iterator m_sourceIt_{ nullptr };

  // TODO(jld-wk): remind me when the token arena is implemented
  std::vector<IrToken> m_tokens_;
};

auto format_ir_token_kind(IrTokenKind kind) -> const char* {
  switch (kind) {
    case IrTokenKind::Char:
      return "<char>";
    case IrTokenKind::Number:
      return "<number>";
    case IrTokenKind::Slot:
      return "<slot>";
    case IrTokenKind::Identifier:
      return "<identifier>";

    case IrTokenKind::KywLoad:
      return "<load>";
    case IrTokenKind::KywStore:
      return "<store>";

    case IrTokenKind::KywAdd:
      return "<add>";
    case IrTokenKind::KywSub:
      return "<sub>";
    case IrTokenKind::KywMul:
      return "<mul>";
    case IrTokenKind::KywDiv:
      return "<div>";

    case IrTokenKind::KywEq:
      return "<eq>";
    case IrTokenKind::KywNe:
      return "<ne>";
    case IrTokenKind::KywLt:
      return "<lt>";
    case IrTokenKind::KywLe:
      return "<le>";
    case IrTokenKind::KywGt:
      return "<gt>";
    case IrTokenKind::KywGe:
      return "<ge>";

    case IrTokenKind::KywJmp:
      return "<jmp>";
    case IrTokenKind::KywCall:
      return "<call>";
    case IrTokenKind::KywJmpIf:
      return "<jmp-if>";
    case IrTokenKind::KywCallIf:
      return "<call-if>";

    case IrTokenKind::KywAlloc:
      return "<alloc>";
    case IrTokenKind::KywFree:
      return "<free>";
    case IrTokenKind::KywLoadAddr:
      return "<load-addr>";
    case IrTokenKind::KywStoreAt:
      return "<store-at>";
    case IrTokenKind::KywStoreAddr:
      return "<store-addr>";

    case IrTokenKind::KywArg:
      return "<arg>";
    case IrTokenKind::KywRet:
      return "<ret>";
    case IrTokenKind::KywEnd:
      return "<end>";

    case IrTokenKind::KywExit:
      return "<exit>";
    case IrTokenKind::KywPrint:
      return "<print>";

    case IrTokenKind::KywU8:
      return "<u8>";
    case IrTokenKind::KywU32:
      return "<u32>";
    case IrTokenKind::KywVoid:
      return "<void>";

    case IrTokenKind::Dot:
      return "<dot>";
    case IrTokenKind::Star:
      return "<star>";
    case IrTokenKind::Colon:
      return "<colon>";
    case IrTokenKind::Comma:
      return "<comma>";
    case IrTokenKind::Minus:
      return "<minus>";
    case IrTokenKind::Arrow:
      return "<arrow>";
    case IrTokenKind::OpenParen:
      return "<open-paren>";
    case IrTokenKind::CloseParen:
      return "<close-paren>";

    case IrTokenKind::Unknown:
      return "<unknown>";
    case IrTokenKind::EndOfFile:
      return "<end-of-file>";
      break;
  }

  return "<unknown>";
}

#endif  // JLD_MCC_IR_TOKENIZER_H