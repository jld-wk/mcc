// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_TOKENIZER_H
#define JLD_MCC_TOKENIZER_H

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "diagnostic/source.h"
#include "diagnostic/source_manager.h"

enum class TokenKind : uint8_t {
  KywDo,
  KywFor,
  KywWhile,
  KywBreak,
  KywContinue,
  KywCase,
  KywDefault,
  KywSwitch,
  KywIf,
  KywElse,
  KywGoto,
  KywReturn,

  KywInt,
  KywShort,
  KywLong,
  KywSigned,
  KywUnsigned,
  KywBool,
  KywChar,
  KywFloat,
  KywDouble,
  KywVoid,
  KywAuto,

  KywEnum,
  KywUnion,
  KywStruct,

  KywConst,
  KywConstexpr,
  KywExtern,
  KywInline,
  KywStatic,
  KywVolatile,
  KywRegister,
  KywRestrict,
  KywAtomic,
  KywThreadLocal,

  KywSizeof,
  KywTypedef,
  KywAlignas,
  KywAlignof,
  KywTypeof,
  KywTypeofUnqual,

  KywTrue,
  KywFalse,
  KywNullptr,

  KywGeneric,
  KywNoreturn,
  KywStaticAssert,

  Number,
  Identifier,

  OpenBracket,
  CloseBracket,
  OpenParen,
  CloseParen,
  OpenBrace,
  CloseBrace,

  Comma,
  Semicolon,
  Dot,
  Ellipsis,

  Plus,
  Minus,
  Star,
  Slash,
  Percent,

  Ampersand,
  Pipe,
  Caret,
  Tilde,
  Exclamation,

  Less,
  Greater,
  Assign,
  Question,
  Colon,

  Increment,
  Decrement,
  Arrow,

  PlusAssign,
  MinusAssign,
  StarAssign,
  SlashAssign,
  PercentAssign,

  AmpersandAssign,
  PipeAssign,
  CaretAssign,

  LogicalAnd,
  LogicalOr,

  LeftShift,
  RightShift,
  LeftShiftAssign,
  RightShiftAssign,

  LessEqual,
  GreaterEqual,
  Equal,
  NotEqual,

  Unknown,
  EndOfFile
};

struct Token {
  std::string_view text;
  SourceRange      range;
  TokenKind        kind;
};

class Tokenizer {
 public:
  auto tokenize(FileId file, std::string_view stream) -> const std::vector<Token>& {
    m_file_ = file;
    m_source_ = stream;
    m_sourceIt_ = stream.begin();

    for (char c = *m_sourceIt_; m_sourceIt_ != stream.end(); c = *m_sourceIt_) {
      m_startLine_ = m_line_;
      m_startColumn_ = m_column_;
      m_startIterated_ = m_iterated_;

      iterate();

      if (c == '\0') {
        push_token(TokenKind::EndOfFile);
        return m_tokens_;
      }

      if (c == '\n') {
        ++m_line_;
        m_column_ = 1;
        continue;
      }

      if (std::isspace(c))
        continue;

      if (std::isalpha(c)) {
        while (is_identifier(*m_sourceIt_)) iterate();

        std::string_view view = str_view();
        TokenKind        kind = keyword_kind(view);

        if (kind != TokenKind::Unknown)
          push_token_str_view(kind, view);
        else
          push_token_str_view(TokenKind::Identifier, view);
        continue;
      }

      if (std::isdigit(c)) {
        while (is_identifier(*m_sourceIt_)) iterate();
        push_token(TokenKind::Number);
        continue;
      }

      if (is_symbol(c)) {
        // TODO(jld-wk): this is not optimized -> even single symbols are treated as if theres the
        // possibility, that they are in a 2 length symbol, for example "()"
        if (is_symbol(*m_sourceIt_)) {
          iterate();

          std::string_view view = str_view();
          TokenKind        kind = len2_symbol_kind(view);

          if (kind == TokenKind::Unknown) {
            m_column_ -= 1;
            m_iterated_ -= 2;
            push_token(len1_symbol_kind(view[0]));
            m_column_ += 1;
            m_iterated_ += 1;
            m_startColumn_ += 1;
            push_token(len1_symbol_kind(view[1]));
            m_iterated_ += 1;
            continue;
          }

          push_token_str_view(kind, view);
          continue;
        }

        push_token(len1_symbol_kind(c));
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
    // TODO(jld-wk): check the C spec
    return std::isalnum(c) || c == '_';
  }

  auto is_symbol(char c) -> bool {
    return c == '[' || c == ']' || c == '(' || c == ')' || c == '{' || c == '}' || c == '+' ||
           c == '-' || c == '*' || c == '/' || c == '%' || c == '&' || c == '|' || c == '^' ||
           c == '~' || c == '!' || c == '<' || c == '>' || c == '=' || c == '?' || c == ':' ||
           c == ',' || c == ';' || c == '.';
  }

  auto str_view() -> std::string_view {
    size_t size = m_iterated_ - m_startIterated_;
    return std::string_view{ m_source_.data() + m_startIterated_, size };
  }

  void push_token(TokenKind kind) {
    uint32_t length = m_column_ - m_startColumn_;
    m_tokens_.push_back(Token{
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

  void push_token_str_view(TokenKind kind, std::string_view view) {
    uint32_t length = m_column_ - m_startColumn_;
    m_tokens_.push_back(Token{
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

  auto len1_symbol_kind(char c) -> TokenKind {
    switch (c) {
      case '[':
        return TokenKind::OpenBracket;
      case ']':
        return TokenKind::CloseBracket;
      case '(':
        return TokenKind::OpenParen;
      case ')':
        return TokenKind::CloseParen;
      case '{':
        return TokenKind::OpenBrace;
      case '}':
        return TokenKind::CloseBrace;

      case '+':
        return TokenKind::Plus;
      case '-':
        return TokenKind::Minus;
      case '*':
        return TokenKind::Star;
      case '/':
        return TokenKind::Slash;
      case '%':
        return TokenKind::Percent;

      case '&':
        return TokenKind::Ampersand;
      case '|':
        return TokenKind::Pipe;
      case '^':
        return TokenKind::Caret;
      case '~':
        return TokenKind::Tilde;
      case '!':
        return TokenKind::Exclamation;

      case '<':
        return TokenKind::Less;
      case '>':
        return TokenKind::Greater;
      case '=':
        return TokenKind::Assign;
      case '?':
        return TokenKind::Question;
      case ':':
        return TokenKind::Colon;

      case ',':
        return TokenKind::Comma;
      case ';':
        return TokenKind::Semicolon;
      case '.':
        return TokenKind::Dot;

      default:
        return TokenKind::Unknown;
    }
  }

  auto len2_symbol_kind(std::string_view symbol) -> TokenKind {
    switch (symbol[0]) {
      case '+':
        if (symbol[1] == '+')
          return TokenKind::Increment;
        if (symbol[1] == '=')
          return TokenKind::PlusAssign;
        break;

      case '-':
        if (symbol[1] == '-')
          return TokenKind::Decrement;
        if (symbol[1] == '=')
          return TokenKind::MinusAssign;
        if (symbol[1] == '>')
          return TokenKind::Arrow;
        break;

      case '*':
        if (symbol[1] == '=')
          return TokenKind::StarAssign;
        break;

      case '/':
        if (symbol[1] == '=')
          return TokenKind::SlashAssign;
        break;

      case '%':
        if (symbol[1] == '=')
          return TokenKind::PercentAssign;
        break;

      case '&':
        if (symbol[1] == '&')
          return TokenKind::LogicalAnd;
        if (symbol[1] == '=')
          return TokenKind::AmpersandAssign;
        break;

      case '|':
        if (symbol[1] == '|')
          return TokenKind::LogicalOr;
        if (symbol[1] == '=')
          return TokenKind::PipeAssign;
        break;

      case '^':
        if (symbol[1] == '=')
          return TokenKind::CaretAssign;
        break;

      case '<':
        if (symbol[1] == '<')
          return TokenKind::LeftShift;
        if (symbol[1] == '=')
          return TokenKind::LessEqual;
        break;

      case '>':
        if (symbol[1] == '>')
          return TokenKind::RightShift;
        if (symbol[1] == '=')
          return TokenKind::GreaterEqual;
        break;

      case '=':
        if (symbol[1] == '=')
          return TokenKind::Equal;
        break;

      case '!':
        if (symbol[1] == '=')
          return TokenKind::NotEqual;
        break;

      default:
        break;
    }

    return TokenKind::Unknown;
  }

  auto keyword_kind(std::string_view view) -> TokenKind {
    if (view == "do")
      return TokenKind::KywDo;
    if (view == "for")
      return TokenKind::KywFor;
    if (view == "while")
      return TokenKind::KywWhile;
    if (view == "break")
      return TokenKind::KywBreak;
    if (view == "continue")
      return TokenKind::KywContinue;
    if (view == "case")
      return TokenKind::KywCase;
    if (view == "default")
      return TokenKind::KywDefault;
    if (view == "switch")
      return TokenKind::KywSwitch;
    if (view == "if")
      return TokenKind::KywIf;
    if (view == "else")
      return TokenKind::KywElse;
    if (view == "goto")
      return TokenKind::KywGoto;
    if (view == "return")
      return TokenKind::KywReturn;

    if (view == "int")
      return TokenKind::KywInt;
    if (view == "short")
      return TokenKind::KywShort;
    if (view == "long")
      return TokenKind::KywLong;
    if (view == "signed")
      return TokenKind::KywSigned;
    if (view == "unsigned")
      return TokenKind::KywUnsigned;
    if (view == "bool")
      return TokenKind::KywBool;
    if (view == "char")
      return TokenKind::KywChar;
    if (view == "float")
      return TokenKind::KywFloat;
    if (view == "double")
      return TokenKind::KywDouble;
    if (view == "void")
      return TokenKind::KywVoid;

    if (view == "auto")
      return TokenKind::KywAuto;
    if (view == "enum")
      return TokenKind::KywEnum;
    if (view == "union")
      return TokenKind::KywUnion;
    if (view == "struct")
      return TokenKind::KywStruct;

    if (view == "const")
      return TokenKind::KywConst;
    if (view == "constexpr")
      return TokenKind::KywConstexpr;
    if (view == "extern")
      return TokenKind::KywExtern;
    if (view == "inline")
      return TokenKind::KywInline;
    if (view == "static")
      return TokenKind::KywStatic;
    if (view == "volatile")
      return TokenKind::KywVolatile;
    if (view == "register")
      return TokenKind::KywRegister;
    if (view == "restrict")
      return TokenKind::KywRestrict;
    // if (view == "_Atomic")
    //   return TokenKind::KywAtomic;
    if (view == "thread_local")
      return TokenKind::KywThreadLocal;

    if (view == "sizeof")
      return TokenKind::KywSizeof;
    if (view == "typedef")
      return TokenKind::KywTypedef;

    if (view == "alignas")
      return TokenKind::KywAlignas;
    if (view == "alignof")
      return TokenKind::KywAlignof;
    if (view == "typeof")
      return TokenKind::KywTypeof;
    if (view == "typeof_unqual")
      return TokenKind::KywTypeofUnqual;

    if (view == "true")
      return TokenKind::KywTrue;
    if (view == "false")
      return TokenKind::KywFalse;
    if (view == "nullptr")
      return TokenKind::KywNullptr;

    // if (view == "_Generic")
    //   return TokenKind::KywGeneric;
    // if (view == "_Noreturn")
    //   return TokenKind::KywNoreturn;
    // if (view == "_Static_assert")
    //   return TokenKind::KywStaticAssert;

    return TokenKind::Unknown;
  }

 private:
  FileId   m_file_{ 0 };
  uint32_t m_line_{ 1 };
  uint32_t m_column_{ 1 };

  uint32_t m_startLine_{ 1 };
  uint32_t m_startColumn_{ 1 };

  size_t m_iterated_{ 0 };
  size_t m_startIterated_{ 0 };

  std::string_view                 m_source_;
  std::string_view::const_iterator m_sourceIt_{ nullptr };

  // TODO(jld-wk): use a token arena -> mhmm probably have to change the arena implementation then
  std::vector<Token> m_tokens_;
};

auto format_token_kind(TokenKind kind) -> const char* {
  switch (kind) {
    case TokenKind::Number:
      return "<number>";
    case TokenKind::Identifier:
      return "<identifier>";

    case TokenKind::KywDo:
      return "<do>";
    case TokenKind::KywFor:
      return "<for>";
    case TokenKind::KywWhile:
      return "<while>";
    case TokenKind::KywBreak:
      return "<break>";
    case TokenKind::KywContinue:
      return "<continue>";
    case TokenKind::KywCase:
      return "<case>";
    case TokenKind::KywDefault:
      return "<default>";
    case TokenKind::KywSwitch:
      return "<switch>";
    case TokenKind::KywIf:
      return "<if>";
    case TokenKind::KywElse:
      return "<else>";
    case TokenKind::KywGoto:
      return "<goto>";
    case TokenKind::KywReturn:
      return "<return>";

    case TokenKind::KywInt:
      return "<int>";
    case TokenKind::KywShort:
      return "<short>";
    case TokenKind::KywLong:
      return "<long>";
    case TokenKind::KywSigned:
      return "<signed>";
    case TokenKind::KywUnsigned:
      return "<unsigned>";
    case TokenKind::KywBool:
      return "<bool>";
    case TokenKind::KywChar:
      return "<char>";
    case TokenKind::KywFloat:
      return "<float>";
    case TokenKind::KywDouble:
      return "<double>";
    case TokenKind::KywVoid:
      return "<void>";

    case TokenKind::KywAuto:
      return "<auto>";
    case TokenKind::KywEnum:
      return "<enum>";
    case TokenKind::KywUnion:
      return "<union>";
    case TokenKind::KywStruct:
      return "<struct>";

    case TokenKind::KywConst:
      return "<const>";
    case TokenKind::KywConstexpr:
      return "<constexpr>";
    case TokenKind::KywExtern:
      return "<extern>";
    case TokenKind::KywInline:
      return "<inline>";
    case TokenKind::KywStatic:
      return "<static>";
    case TokenKind::KywVolatile:
      return "<volatile>";
    case TokenKind::KywRegister:
      return "<register>";
    case TokenKind::KywRestrict:
      return "<restrict>";
    case TokenKind::KywAtomic:
      return "<atomic>";
    case TokenKind::KywThreadLocal:
      return "<thread-local>";

    case TokenKind::KywSizeof:
      return "<sizeof>";
    case TokenKind::KywTypedef:
      return "<typedef>";

    case TokenKind::KywAlignas:
      return "<alignas>";
    case TokenKind::KywAlignof:
      return "<alignof>";
    case TokenKind::KywTypeof:
      return "<typeof>";
    case TokenKind::KywTypeofUnqual:
      return "<typeof-unqual>";

    case TokenKind::KywTrue:
      return "<true>";
    case TokenKind::KywFalse:
      return "<false>";
    case TokenKind::KywNullptr:
      return "<nullptr>";

    case TokenKind::KywGeneric:
      return "<generic>";
    case TokenKind::KywNoreturn:
      return "<noreturn>";
    case TokenKind::KywStaticAssert:
      return "<static-assert>";

    case TokenKind::OpenBracket:
      return "<open-bracket>";
    case TokenKind::CloseBracket:
      return "<close-bracket>";
    case TokenKind::OpenParen:
      return "<open-paren>";
    case TokenKind::CloseParen:
      return "<close-paren>";
    case TokenKind::OpenBrace:
      return "<open-brace>";
    case TokenKind::CloseBrace:
      return "<close-brace>";

    case TokenKind::Comma:
      return "<comma>";
    case TokenKind::Semicolon:
      return "<semicolon>";
    case TokenKind::Dot:
      return "<dot>";
    case TokenKind::Ellipsis:
      return "<ellipsis>";

    case TokenKind::Plus:
      return "<plus>";
    case TokenKind::Minus:
      return "<minus>";
    case TokenKind::Star:
      return "<star>";
    case TokenKind::Slash:
      return "<slash>";
    case TokenKind::Percent:
      return "<percent>";

    case TokenKind::Ampersand:
      return "<ampersand>";
    case TokenKind::Pipe:
      return "<pipe>";
    case TokenKind::Caret:
      return "<caret>";
    case TokenKind::Tilde:
      return "<tilde>";
    case TokenKind::Exclamation:
      return "<exclamation>";

    case TokenKind::Less:
      return "<less>";
    case TokenKind::Greater:
      return "<greater>";
    case TokenKind::Assign:
      return "<assign>";
    case TokenKind::Question:
      return "<question>";
    case TokenKind::Colon:
      return "<colon>";

    case TokenKind::Increment:
      return "<increment>";
    case TokenKind::Decrement:
      return "<decrement>";
    case TokenKind::Arrow:
      return "<arrow>";

    case TokenKind::PlusAssign:
      return "<plus-assign>";
    case TokenKind::MinusAssign:
      return "<minus-assign>";
    case TokenKind::StarAssign:
      return "<star-assign>";
    case TokenKind::SlashAssign:
      return "<slash-assign>";
    case TokenKind::PercentAssign:
      return "<percent-assign>";

    case TokenKind::AmpersandAssign:
      return "<ampersand-assign>";
    case TokenKind::PipeAssign:
      return "<pipe-assign>";
    case TokenKind::CaretAssign:
      return "<caret-assign>";

    case TokenKind::LogicalAnd:
      return "<logical-and>";
    case TokenKind::LogicalOr:
      return "<logical-or>";

    case TokenKind::LeftShift:
      return "<left-shift>";
    case TokenKind::RightShift:
      return "<right-shift>";
    case TokenKind::LeftShiftAssign:
      return "<left-shift-assign>";
    case TokenKind::RightShiftAssign:
      return "<right-shift-assign>";

    case TokenKind::LessEqual:
      return "<less-equal>";
    case TokenKind::GreaterEqual:
      return "<greater-equal>";
    case TokenKind::Equal:
      return "<equal>";
    case TokenKind::NotEqual:
      return "<not-equal>";

    case TokenKind::Unknown:
      return "<unknown>";
    case TokenKind::EndOfFile:
      return "<end-of-file>";
  }

  return "<unknown>";
}

#endif  // JLD_MCC_TOKENIZE_H