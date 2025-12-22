#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<InlineCStmt> Parser::inlineCStatement() {
  int line = previous().line;

  if (!check(TokenType::IDENTIFIER) || peek().lexeme != "C") {
    error("Expected 'C' after 'inline'", peek().line);
    return nullptr;
  }
  advance();

  if (!match({TokenType::LBRACKET})) {
    error("Expected '[' after 'C'", peek().line);
    return nullptr;
  }

  if (!match({TokenType::LBRACKET})) {
    error("Expected second '[' for C[[ syntax", peek().line);
    return nullptr;
  }

  skipNewlines();

  std::string cCode;

  while (!isAtEnd()) {
    if (check(TokenType::RBRACKET)) {
      size_t lookahead = current + 1;
      if (lookahead < tokens.size() && 
          tokens[lookahead].type == TokenType::RBRACKET) {
        break;
      }
    }

    Token token = advance();
    if (token.type == TokenType::NEWLINE) {
      cCode += "\n";
      continue;
    }

    if (token.type == TokenType::STRING) {
      cCode += "\"" + std::get<std::string>(token.literal) + "\"";
    } else {
      cCode += token.lexeme;
    }

    if (!isAtEnd() && peek().type != TokenType::RBRACKET &&
        peek().type != TokenType::NEWLINE) {
      cCode += " ";
    }
  }

  if (!match({TokenType::RBRACKET})) {
    error("Expected ']]' to close inline C block", peek().line);
    return nullptr;
  }

  if (!match({TokenType::RBRACKET})) {
    error("Expected second ']' for ]] syntax", peek().line);
    return nullptr;
  }

  while (!cCode.empty() && (cCode.back() == ' ' || cCode.back() == '\n')) {
    cCode.pop_back();
  }

  auto inlineC = std::make_unique<InlineCStmt>(cCode);
  inlineC->line = line;
  skipNewlines();
  return inlineC;
}

} // namespace HolyLua
