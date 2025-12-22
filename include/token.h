#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace HolyLua {

enum class TokenType {
  // keywords
  LOCAL,
  GLOBAL,
  CONST,
  PRINT,
  IF,
  THEN,
  ELSE,
  ELSEIF,
  END,
  FUNCTION,
  RETURN,
  AND,
  OR,
  NOT,
  WHILE,
  DO,
  FOR,
  REPEAT,
  UNTIL,
  INLINE,
  STRUCT,
  ENUM,
  CLASS,
  PUBLIC,
  PRIVATE,
  STATIC,
  SELF,
  C,

  // types
  TYPE_NUMBER,
  TYPE_STRING,
  TYPE_BOOL,

  // literals
  NUMBER,
  STRING,
  TRUE,
  FALSE,
  NIL,

  // identifiers
  IDENTIFIER,

  // operators
  ASSIGN,
  PLUS_ASSIGN,
  MINUS_ASSIGN,
  STAR_ASSIGN,
  SLASH_ASSIGN,
  PERCENT_ASSIGN,
  DOUBLE_STAR_ASSIGN,
  DOUBLE_SLASH_ASSIGN,
  COLON,
  PLUS,
  MINUS,
  STAR,
  DOUBLE_STAR,
  SLASH,
  DOUBLE_SLASH,
  PERCENT,
  EQUAL,
  NOT_EQUAL,
  LESS,
  LESS_EQUAL,
  GREATER,
  GREATER_EQUAL,
  BANG,
  QUESTION,
  DOUBLE_QUESTION,

  // Punctuation
  LPAREN,
  RPAREN,
  LBRACKET,
  RBRACKET,
  CONCAT,
  DOT,
  COMMA,
  NEWLINE,
  SEMI_COLON,
  LBRACE,
  RBRACE,
  END_OF_FILE
};

struct Token {
  TokenType type;
  std::string lexeme;
  std::variant<int64_t, double, std::string> literal;
  int line;

  Token(TokenType t, std::string lex, int l);
  Token(TokenType t, std::string lex, int64_t num, int l);
  Token(TokenType t, std::string lex, double num, int l);
  Token(TokenType t, std::string lex, std::string str, int l);
};

std::string tokenTypeToString(TokenType type);

} // namespace HolyLua