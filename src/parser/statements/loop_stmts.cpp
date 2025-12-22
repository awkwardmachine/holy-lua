#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<WhileStmt> Parser::whileStatement() {
  int whileLine = previous().line;

  auto condition = expression();

  if (!match({TokenType::DO})) {
    error("Expected 'do' after while condition", peek().line);
    return nullptr;
  }

  skipNewlines();

  auto whileStmt = std::make_unique<WhileStmt>(std::move(condition));
  whileStmt->line = whileLine;

  // parse while loop body
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      whileStmt->body.push_back(std::move(stmt));
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close while statement", peek().line);
    return nullptr;
  }

  skipNewlines();
  return whileStmt;
}

std::unique_ptr<RepeatStmt> Parser::repeatStatement() {
  int repeatLine = previous().line;
  auto repeatStmt = std::make_unique<RepeatStmt>(nullptr);
  repeatStmt->line = repeatLine;

  skipNewlines();

  // parse repeat loop body
  while (!check(TokenType::UNTIL) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::UNTIL))
      break;

    auto stmt = statement();
    if (stmt) {
      repeatStmt->body.push_back(std::move(stmt));
    }
  }

  if (!match({TokenType::UNTIL})) {
    error("Expected 'until' after repeat body", peek().line);
    return nullptr;
  }

  // parse the until condition
  repeatStmt->condition = expression();

  skipNewlines();
  return repeatStmt;
}

std::unique_ptr<ForStmt> Parser::forStatement() {
  int forLine = previous().line;

  if (!match({TokenType::LOCAL})) {
    error("Expected 'local' in for loop declaration", peek().line);
    return nullptr;
  }

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected variable name in for loop", peek().line);
    return nullptr;
  }

  Token varName = advance();

  if (!match({TokenType::ASSIGN})) {
    error("Expected '=' after for loop variable", peek().line);
    return nullptr;
  }

  auto start = expression();

  if (!match({TokenType::COMMA})) {
    error("Expected ',' after start value", peek().line);
    return nullptr;
  }

  auto end = expression();

  std::unique_ptr<Expr> step = nullptr;
  if (match({TokenType::COMMA})) {
    step = expression();
  }

  skipNewlines();

  auto forStmt = std::make_unique<ForStmt>(varName.lexeme, std::move(start),
                                           std::move(end), std::move(step));
  forStmt->line = forLine;

  // parse for loop body
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      forStmt->body.push_back(std::move(stmt));
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close for loop", peek().line);
    return nullptr;
  }

  skipNewlines();
  return forStmt;
}

} // namespace HolyLua
