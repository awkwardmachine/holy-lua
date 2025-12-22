#pragma once
#include "token.h"
#include <unordered_map>
#include <vector>

namespace HolyLua {

class Lexer {
public:
  explicit Lexer(const std::string &source);
  std::vector<Token> scanTokens();
  bool hasErrors() const { return errorCount > 0; }

private:
  std::string source;
  std::vector<Token> tokens;
  std::unordered_map<std::string, TokenType> keywords;

  int current = 0;
  int start = 0;
  int line = 1;
  int errorCount = 0;

  bool isAtEnd();
  char advance();
  char peek();
  char peekNext();

  void scanToken();
  void string();
  void number();
  void identifier();

  bool isDigit(char c);
  bool isAlpha(char c);
  bool isAlphaNumeric(char c);
  void error(const std::string &msg, int line);
};

} // namespace HolyLua