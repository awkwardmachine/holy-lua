#include "../../include/token.h"

namespace HolyLua {

Token::Token(TokenType t, std::string lex, int l)
    : type(t), lexeme(lex), line(l) {}

Token::Token(TokenType t, std::string lex, int64_t num, int l)
    : type(t), lexeme(lex), literal(num), line(l) {}

Token::Token(TokenType t, std::string lex, double num, int l)
    : type(t), lexeme(lex), literal(num), line(l) {}

Token::Token(TokenType t, std::string lex, std::string str, int l)
    : type(t), lexeme(lex), literal(str), line(l) {}

std::string tokenTypeToString(TokenType type) {
  switch (type) {
  case TokenType::LOCAL:
    return "LOCAL";
  case TokenType::GLOBAL:
    return "GLOBAL";
  case TokenType::CONST:
    return "CONST";
  case TokenType::PRINT:
    return "PRINT";
  case TokenType::NUMBER:
    return "NUMBER";
  case TokenType::STRING:
    return "STRING";
  case TokenType::IDENTIFIER:
    return "IDENTIFIER";
  case TokenType::ASSIGN:
    return "ASSIGN";
  case TokenType::LPAREN:
    return "LPAREN";
  case TokenType::RPAREN:
    return "RPAREN";
  default:
    return "UNKNOWN";
  }
}

} // namespace HolyLua