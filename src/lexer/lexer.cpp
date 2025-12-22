#include "../../include/lexer.h"
#include <iostream>
#include <stdexcept>

namespace HolyLua {

Lexer::Lexer(const std::string &source)
    : source(source), current(0), start(0), line(1), errorCount(0) {
  keywords["local"] = TokenType::LOCAL;
  keywords["global"] = TokenType::GLOBAL;
  keywords["const"] = TokenType::CONST;
  keywords["print"] = TokenType::PRINT;
  keywords["if"] = TokenType::IF;
  keywords["then"] = TokenType::THEN;
  keywords["else"] = TokenType::ELSE;
  keywords["elseif"] = TokenType::ELSEIF;
  keywords["end"] = TokenType::END;
  keywords["function"] = TokenType::FUNCTION;
  keywords["return"] = TokenType::RETURN;
  keywords["number"] = TokenType::TYPE_NUMBER;
  keywords["string"] = TokenType::TYPE_STRING;
  keywords["bool"] = TokenType::TYPE_BOOL;
  keywords["true"] = TokenType::TRUE;
  keywords["false"] = TokenType::FALSE;
  keywords["nil"] = TokenType::NIL;
  keywords["and"] = TokenType::AND;
  keywords["or"] = TokenType::OR;
  keywords["not"] = TokenType::NOT;
  keywords["while"] = TokenType::WHILE;
  keywords["do"] = TokenType::DO;
  keywords["for"] = TokenType::FOR;
  keywords["repeat"] = TokenType::REPEAT;
  keywords["until"] = TokenType::UNTIL;
  keywords["inline"] = TokenType::INLINE;
  keywords["struct"] = TokenType::STRUCT;
  keywords["enum"] = TokenType::ENUM;
  keywords["class"] = TokenType::CLASS;
  keywords["public"] = TokenType::PUBLIC;
  keywords["private"] = TokenType::PRIVATE;
  keywords["static"] = TokenType::STATIC;
  keywords["self"] = TokenType::SELF;
}

void Lexer::error(const std::string &msg, int line) {
  std::cerr << "\033[1;31mLexer Error:\033[0m " << msg << " on line " << line
            << "\n";
  errorCount++;
}

std::vector<Token> Lexer::scanTokens() {
  while (!isAtEnd()) {
    start = current;
    scanToken();
  }
  tokens.emplace_back(TokenType::END_OF_FILE, "", line);
  return tokens;
}

bool Lexer::isAtEnd() {
  return static_cast<size_t>(current) >= source.length();
}

char Lexer::advance() { return source[current++]; }

char Lexer::peek() {
  if (isAtEnd())
    return '\0';
  return source[current];
}

char Lexer::peekNext() {
  if (static_cast<size_t>(current) + 1 >= source.length())
    return '\0';
  return source[current + 1];
}

void Lexer::scanToken() {
  char c = advance();
  switch (c) {
  case ' ':
  case '\r':
  case '\t':
    break;
  case '\n':
    tokens.emplace_back(TokenType::NEWLINE, "\\n", line);
    line++;
    break;
  case ';':
    tokens.emplace_back(TokenType::SEMI_COLON, ";", line);
    break;
  case '{':
    tokens.emplace_back(TokenType::LBRACE, "{", line);
    break;
  case '}':
    tokens.emplace_back(TokenType::RBRACE, "}", line);
    break;
  case '[':
    tokens.emplace_back(TokenType::LBRACKET, "[", line);
    break;
  case ']':
    tokens.emplace_back(TokenType::RBRACKET, "]", line);
    break;
  case '=':
    if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::EQUAL, "==", line);
    } else {
      tokens.emplace_back(TokenType::ASSIGN, "=", line);
    }
    break;
  case '!':
    if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::NOT_EQUAL, "!=", line);
    } else {
      tokens.emplace_back(TokenType::BANG, "!", line);
    }
    break;
  case '<':
    if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::LESS_EQUAL, "<=", line);
    } else {
      tokens.emplace_back(TokenType::LESS, "<", line);
    }
    break;
  case '>':
    if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::GREATER_EQUAL, ">=", line);
    } else {
      tokens.emplace_back(TokenType::GREATER, ">", line);
    }
    break;
  case ':':
    tokens.emplace_back(TokenType::COLON, ":", line);
    break;
  case '?':
    if (peek() == '?') {
      advance();
      tokens.emplace_back(TokenType::DOUBLE_QUESTION, "??", line);
    } else {
      tokens.emplace_back(TokenType::QUESTION, "?", line);
    }
    break;
  case '(':
    tokens.emplace_back(TokenType::LPAREN, "(", line);
    break;
  case ')':
    tokens.emplace_back(TokenType::RPAREN, ")", line);
    break;
  case ',':
    tokens.emplace_back(TokenType::COMMA, ",", line);
    break;
  case '+':
    if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::PLUS_ASSIGN, "+=", line);
    } else {
      tokens.emplace_back(TokenType::PLUS, "+", line);
    }
    break;
  case '*':
    if (peek() == '*') {
      advance();
      if (peek() == '=') {
        advance();
        tokens.emplace_back(TokenType::DOUBLE_STAR_ASSIGN, "**=", line);
      } else {
        tokens.emplace_back(TokenType::DOUBLE_STAR, "**", line);
      }
    } else if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::STAR_ASSIGN, "*=", line);
    } else {
      tokens.emplace_back(TokenType::STAR, "*", line);
    }
    break;
  case '/':
    if (peek() == '/') {
      advance();
      if (peek() == '=') {
        advance();
        tokens.emplace_back(TokenType::DOUBLE_SLASH_ASSIGN, "//=", line);
      } else {
        tokens.emplace_back(TokenType::DOUBLE_SLASH, "//", line);
      }
    } else if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::SLASH_ASSIGN, "/=", line);
    } else {
      tokens.emplace_back(TokenType::SLASH, "/", line);
    }
    break;
  case '%':
    if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::PERCENT_ASSIGN, "%=", line);
    } else {
      tokens.emplace_back(TokenType::PERCENT, "%", line);
    }
    break;
  case '-':
    if (peek() == '-') {
      advance();
      while (peek() != '\n' && !isAtEnd())
        advance();
    } else if (peek() == '=') {
      advance();
      tokens.emplace_back(TokenType::MINUS_ASSIGN, "-=", line);
    } else {
      tokens.emplace_back(TokenType::MINUS, "-", line);
    }
    break;
  case '.':
    if (peek() == '.') {
      advance();
      tokens.emplace_back(TokenType::CONCAT, "..", line);
    } else {
      tokens.emplace_back(TokenType::DOT, ".", line);
    }
    break;
  case '"':
    string();
    break;
  default:
    if (isDigit(c)) {
      number();
    } else if (isAlpha(c)) {
      identifier();
    }
    break;
  }
}

void Lexer::string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n')
      line++;
    advance();
  }
  if (isAtEnd())
    return;
  advance();
  std::string value = source.substr(start + 1, current - start - 2);
  tokens.emplace_back(TokenType::STRING, value, value, line);
}

void Lexer::number() {
  bool isFloat = false;

  while (isDigit(peek()))
    advance();

  if (peek() == '.' && isDigit(peekNext())) {
    isFloat = true;
    advance();
    while (isDigit(peek()))
      advance();
  }

  std::string value = source.substr(start, current - start);

  try {
    if (isFloat) {
      double num = std::stod(value);
      tokens.emplace_back(TokenType::NUMBER, value, num, line);
    } else {
      if (value == "9223372036854775808") {
        tokens.emplace_back(TokenType::NUMBER, value,
                            static_cast<int64_t>(9223372036854775808ULL), line);
      } else {
        int64_t num = std::stoll(value);
        tokens.emplace_back(TokenType::NUMBER, value, num, line);
      }
    }
  } catch (const std::out_of_range &e) {
    std::cerr << "Error on line " << line << ": Number '" << value
              << "' is out of range\n";
    errorCount++;
    if (isFloat) {
      tokens.emplace_back(TokenType::NUMBER, value, 0.0, line);
    } else {
      tokens.emplace_back(TokenType::NUMBER, value, static_cast<int64_t>(0),
                          line);
    }
  } catch (const std::invalid_argument &e) {
    std::cerr << "Error on line " << line << ": Invalid number format '"
              << value << "'\n";
    errorCount++;
    if (isFloat) {
      tokens.emplace_back(TokenType::NUMBER, value, 0.0, line);
    } else {
      tokens.emplace_back(TokenType::NUMBER, value, static_cast<int64_t>(0),
                          line);
    }
  }
}

void Lexer::identifier() {
  while (isAlphaNumeric(peek()))
    advance();
  std::string text = source.substr(start, current - start);
  TokenType type =
      keywords.count(text) ? keywords[text] : TokenType::IDENTIFIER;
  tokens.emplace_back(type, text, line);
}

bool Lexer::isDigit(char c) { return c >= '0' && c <= '9'; }

bool Lexer::isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isAlphaNumeric(char c) { return isAlpha(c) || isDigit(c); }

} // namespace HolyLua