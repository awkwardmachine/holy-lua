#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<WhileStmt> Parser::whileStatement() {
  int whileLine = previous().line;

  auto condition = expression();
  if (!condition) {
    synchronize();
    return nullptr;
  }

  if (!match({TokenType::DO})) {
    error("Expected 'do' after while condition", peek().line);
    synchronize();
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
    } else {
      if (!check(TokenType::END)) {
        advance();
      }
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close while statement", peek().line);
    synchronize();
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
    } else {
      if (!check(TokenType::UNTIL)) {
        advance();
      }
    }
  }

  if (!match({TokenType::UNTIL})) {
    error("Expected 'until' after repeat body", peek().line);
    synchronize();
    return nullptr;
  }

  // parse the until condition
  repeatStmt->condition = expression();
  if (!repeatStmt->condition) {
    synchronize();
    return nullptr;
  }

  skipNewlines();
  return repeatStmt;
}

std::unique_ptr<ForStmt> Parser::forStatement() {
  int forLine = previous().line;

  if (!match({TokenType::LOCAL})) {
    error("Expected 'local' in for loop declaration", peek().line);
    while (!check(TokenType::END) && !isAtEnd()) {
      advance();
    }
    if (match({TokenType::END})) {
      skipNewlines();
    }
    return nullptr;
  }

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected variable name in for loop", peek().line);
    synchronize();
    return nullptr;
  }

  Token varName = advance();

  if (!match({TokenType::ASSIGN})) {
    error("Expected '=' after for loop variable", peek().line);
    synchronize();
    return nullptr;
  }

  auto start = expression();
  if (!start) {
    synchronize();
    return nullptr;
  }

  if (!match({TokenType::COMMA})) {
    error("Expected ',' after start value", peek().line);
    synchronize();
    return nullptr;
  }

  auto end = expression();
  if (!end) {
    synchronize();
    return nullptr;
  }

  std::unique_ptr<Expr> step = nullptr;
  if (match({TokenType::COMMA})) {
    step = expression();
    if (!step) {
      synchronize();
      return nullptr;
    }
  }

  if (!match({TokenType::DO})) {
    error("Expected 'do' after for loop range", peek().line);
    synchronize();
    return nullptr;
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
    } else {
      if (!check(TokenType::END)) {
        advance();
      }
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close for loop", peek().line);
    synchronize();
    return nullptr;
  }

  skipNewlines();
  return forStmt;
}

} // namespace HolyLua