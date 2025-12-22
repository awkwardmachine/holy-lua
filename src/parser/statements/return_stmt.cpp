#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<ReturnStmt> Parser::returnStatement() {
  int returnLine = previous().line;
  auto returnStmt = std::make_unique<ReturnStmt>();

  if (!check(TokenType::NEWLINE) && !check(TokenType::END) && !isAtEnd()) {
    returnStmt->value = expression();
  }

  skipNewlines();
  returnStmt->line = returnLine;
  return returnStmt;
}

} // namespace HolyLua
