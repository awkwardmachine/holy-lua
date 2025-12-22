#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<IfStmt> Parser::ifStatement() {
  int ifLine = previous().line;

  auto condition = expression();

  if (!match({TokenType::THEN})) {
    error("Expected 'then' after if condition", peek().line);
    return nullptr;
  }

  skipNewlines();

  auto ifStmt = std::make_unique<IfStmt>(std::move(condition));
  ifStmt->line = ifLine;

  // parse then block
  while (!check(TokenType::ELSEIF) && !check(TokenType::ELSE) && !check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::ELSEIF) || check(TokenType::ELSE) || check(TokenType::END))
      break;
    auto stmt = statement();
    if (stmt) {
      ifStmt->thenBlock.push_back(std::move(stmt));
    }
  }

  // parse elseif branches
  while (match({TokenType::ELSEIF})) {
    auto elseifCondition = expression();

    if (!match({TokenType::THEN})) {
      error("Expected 'then' after elseif condition", peek().line);
      return nullptr;
    }

    skipNewlines();

    std::vector<std::unique_ptr<ASTNode>> elseifBlock;

    while (!check(TokenType::ELSEIF) && !check(TokenType::ELSE) && !check(TokenType::END) && !isAtEnd()) {
      skipNewlines();
      if (check(TokenType::ELSEIF) || check(TokenType::ELSE) || check(TokenType::END))
        break;
      auto stmt = statement();
      if (stmt) {
        elseifBlock.push_back(std::move(stmt));
      }
    }

    ifStmt->elseifBranches.emplace_back(std::move(elseifCondition), std::move(elseifBlock));
  }

  // parse optional else block
  if (match({TokenType::ELSE})) {
    skipNewlines();
    while (!check(TokenType::END) && !isAtEnd()) {
      skipNewlines();
      if (check(TokenType::END))
        break;
      auto stmt = statement();
      if (stmt) {
        ifStmt->elseBlock.push_back(std::move(stmt));
      }
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close if statement", peek().line);
    return nullptr;
  }

  skipNewlines();

  return ifStmt;
}

} // namespace HolyLua
