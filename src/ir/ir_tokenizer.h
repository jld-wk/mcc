// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_IR_TOKENIZER_H
#define JLD_MCC_IR_TOKENIZER_H

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

  KywPush,
  KywPop,

  KywLoad,
  KywStore,

  KywAdd,
  KywSub,
  KywMul,
  KywDiv,

  KywDup,
  KywExit,

  KywCall,
  KywCallT,
  KywCallF,

  KywJmp,
  KywJmpT,
  KywJmpF,

  KywDbgDump,

  KywEq,
  KywNe,
  KywLt,
  KywLe,
  KywGt,
  KywGe,

  Colon,
  Semicolon,

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
    if (view == "push")
      return IrTokenKind::KywPush;
    if (view == "pop")
      return IrTokenKind::KywPop;

    if (view == "store")
      return IrTokenKind::KywStore;
    if (view == "load")
      return IrTokenKind::KywLoad;

    if (view == "add")
      return IrTokenKind::KywAdd;
    if (view == "sub")
      return IrTokenKind::KywSub;
    if (view == "mul")
      return IrTokenKind::KywMul;
    if (view == "div")
      return IrTokenKind::KywDiv;

    if (view == "dup")
      return IrTokenKind::KywDup;
    if (view == "exit")
      return IrTokenKind::KywExit;

    if (view == "call")
      return IrTokenKind::KywCall;
    if (view == "call_t")
      return IrTokenKind::KywCallT;
    if (view == "call_f")
      return IrTokenKind::KywCallF;

    if (view == "jmp")
      return IrTokenKind::KywJmp;
    if (view == "jmp_t")
      return IrTokenKind::KywJmpT;
    if (view == "jmp_f")
      return IrTokenKind::KywJmpF;

    if (view == "dbg_dump")
      return IrTokenKind::KywDbgDump;

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

    case IrTokenKind::KywPush:
      return "<push>";
    case IrTokenKind::KywPop:
      return "<pop>";

    case IrTokenKind::KywStore:
      return "<store>";
    case IrTokenKind::KywLoad:
      return "<load>";

    case IrTokenKind::KywAdd:
      return "<add>";
    case IrTokenKind::KywSub:
      return "<sub>";
    case IrTokenKind::KywMul:
      return "<mul>";
    case IrTokenKind::KywDiv:
      return "<div>";

    case IrTokenKind::KywDup:
      return "<dup>";
    case IrTokenKind::KywExit:
      return "<exit>";

    case IrTokenKind::KywCall:
      return "<call>";
    case IrTokenKind::KywCallT:
      return "<call_t>";
    case IrTokenKind::KywCallF:
      return "<call_f>";

    case IrTokenKind::KywJmp:
      return "<jmp>";
    case IrTokenKind::KywJmpT:
      return "<jmp_t>";
    case IrTokenKind::KywJmpF:
      return "<jmp_f>";

    case IrTokenKind::KywDbgDump:
      return "<dbg_dump>";

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

    case IrTokenKind::Colon:
      return "<colon>";
    case IrTokenKind::Semicolon:
      return "<semicolon>";

    case IrTokenKind::Unknown:
      return "<unknown>";
    case IrTokenKind::EndOfFile:
      return "<end-of-file>";
      break;
  }

  return "<unknown>";
}

#endif  // JLD_MCC_IR_TOKENIZER_H