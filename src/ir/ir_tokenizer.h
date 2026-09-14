// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_TOKENIZER_H
#define JLD_MCC_IR_TOKENIZER_H

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "diagnostic/source.h"

enum class IrTokenKind : uint8_t {
  Number,
  Identifier,
  Slot,

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
  KywStorePtr,
  KywStoreAddr,

  KywArg,
  KywRet,
  KywEnd,
  KywVoid,

  KywExit,

  KywDumpD,
  KywDumpC,

  Colon,
  Comma,
  Minus,

  OpenParen,
  CloseParen,

  Unknown,
  EndOfFile
};

struct IrToken {
  IrTokenKind      kind;
  SourceRange      source;
  std::string_view text;
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

      if (c == '-') {
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

      // TODO(jld-wk): store 0, [r:rax], so you can then basically bypass
      // the register allocation and also then for globals, store 0, [g:my_literal]
      if (c == '[') {
        while (is_identifier(*m_sourceIt_)) iterate();

        if (*m_sourceIt_ == ']') {
          iterate();
          push_token(IrTokenKind::Slot);
          continue;
        }

        if (*m_sourceIt_ != ':') {
          assert(false);
          // TODO(jld-wk): print diagnostic
        }

        iterate();

        while (is_identifier(*m_sourceIt_)) iterate();
        if (*m_sourceIt_ != ']') {
          assert(false);
          // TODO(jld-wk): print diagnostic
        }

        iterate();
        push_token(IrTokenKind::Slot);
        continue;
      }

      // TODO(jld-wk): print diagnostic
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

  void push_token(IrTokenKind kind) {
    m_tokens_.push_back(IrToken{
        .kind = kind,
        .source =
            SourceRange{
                .file = m_file_,
                .begin = SourceLocation{ .line = m_startLine_, .column = m_startColumn_ },
                .end = SourceLocation{ .line = m_line_, .column = m_column_ },
                .beginIt = m_startIterated_,
                .endIt = m_iterated_,
            },
        .text = str_view(),
    });
  }

  void push_token_str_view(IrTokenKind kind, std::string_view view) {
    m_tokens_.push_back(IrToken{
        .kind = kind,
        .source =
            SourceRange{
                .file = m_file_,
                .begin = SourceLocation{ .line = m_startLine_, .column = m_startColumn_ },
                .end = SourceLocation{ .line = m_line_, .column = m_column_ },
                .beginIt = m_startIterated_,
                .endIt = m_iterated_,
            },
        .text = view,
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
    if (view == "store_ptr")
      return IrTokenKind::KywStorePtr;

    if (view == "exit")
      return IrTokenKind::KywExit;
    if (view == "void")
      return IrTokenKind::KywVoid;

    if (view == "end")
      return IrTokenKind::KywEnd;

    if (view == "ret")
      return IrTokenKind::KywRet;
    if (view == "arg")
      return IrTokenKind::KywArg;

    if (view == "dump_d")
      return IrTokenKind::KywDumpD;
    if (view == "dump_c")
      return IrTokenKind::KywDumpC;

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
    case IrTokenKind::Number:
      return "<number>";
    case IrTokenKind::Identifier:
      return "<identifier>";
    case IrTokenKind::Slot:
      return "<slot>";

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
      return "<jmp_if>";
    case IrTokenKind::KywCallIf:
      return "<call_if>";

    case IrTokenKind::KywAlloc:
      return "<alloc>";
    case IrTokenKind::KywFree:
      return "<free>";
    case IrTokenKind::KywLoadAddr:
      return "<load_addr>";
    case IrTokenKind::KywStorePtr:
      return "<store_ptr>";
    case IrTokenKind::KywStoreAddr:
      return "<store_addr>";

    case IrTokenKind::KywArg:
      return "<arg>";
    case IrTokenKind::KywRet:
      return "<ret>";

    case IrTokenKind::KywEnd:
      return "<end>";
    case IrTokenKind::KywVoid:
      return "<void>";

    case IrTokenKind::KywExit:
      return "<exit>";

    case IrTokenKind::KywDumpD:
      return "<dump_int>";
    case IrTokenKind::KywDumpC:
      return "<dump_char>";

    case IrTokenKind::Colon:
      return "<colon>";
    case IrTokenKind::Comma:
      return "<comma>";
    case IrTokenKind::Minus:
      return "<minus>";
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