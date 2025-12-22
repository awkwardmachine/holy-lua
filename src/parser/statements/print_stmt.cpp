#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<PrintStmt> Parser::printStatement() {
  int printLine = previous().line;

  if (!match({TokenType::LPAREN})) {
    error("Expected '(' after 'print'", printLine);
    return nullptr;
  }

  std::vector<PrintArg> args;

  if (!check(TokenType::RPAREN)) {
    do {
      auto expr = expression();
      if (auto *varExpr = dynamic_cast<VarExpr *>(expr.get())) {
        args.emplace_back(varExpr->name);
      } else {
        args.emplace_back(std::move(expr));
      }
    } while (match({TokenType::COMMA}));
  }

  if (!match({TokenType::RPAREN})) {
    error("Expected ')' after print arguments", peek().line);
    return nullptr;
  }

  skipNewlines();

  auto stmt = std::make_unique<PrintStmt>(std::move(args));
  stmt->line = printLine;
  return stmt;
}

} // namespace HolyLua
