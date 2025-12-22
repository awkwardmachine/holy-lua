#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<Expr> Parser::primary() {
  int line = peek().line;

  if (match({TokenType::NUMBER})) {
    const auto &lit = previous().literal;
    auto expr = std::unique_ptr<LiteralExpr>(nullptr);
    if (std::holds_alternative<int64_t>(lit)) {
      expr = std::make_unique<LiteralExpr>(std::get<int64_t>(lit));
    } else if (std::holds_alternative<double>(lit)) {
      expr = std::make_unique<LiteralExpr>(std::get<double>(lit));
    }
    if (expr) {
      expr->line = line;
      return expr;
    }
  }

  if (match({TokenType::FUNCTION})) {
    return lambdaExpression(line);
  }

  if (match({TokenType::STRING})) {
    auto expr = std::make_unique<LiteralExpr>(
        std::get<std::string>(previous().literal));
    expr->line = line;
    return expr;
  }

  if (match({TokenType::TRUE})) {
    auto expr = std::make_unique<LiteralExpr>(true);
    expr->line = line;
    return expr;
  }

  if (match({TokenType::FALSE})) {
    auto expr = std::make_unique<LiteralExpr>(false);
    expr->line = line;
    return expr;
  }

  if (match({TokenType::NIL})) {
    auto expr = std::make_unique<NilExpr>();
    expr->line = line;
    return expr;
  }

  if (match({TokenType::SELF})) {
    auto expr = std::make_unique<SelfExpr>();
    expr->line = line;
    return expr;
  }

  if (match({TokenType::IDENTIFIER})) {
    Token ident = previous();

    // check if it's a class instantiation
    if (declaredClasses.count(ident.lexeme) > 0 && check(TokenType::LPAREN)) {
      return classInstantiation();
    }

    if (check(TokenType::LBRACE)) {
      // check if this identifier is a known struct
      if (declaredStructs.count(ident.lexeme) > 0) {
        // this is a struct constructor
        auto constructor = structConstructor();
        if (constructor) {
          return constructor;
        }
      } else {
        auto expr = std::make_unique<VarExpr>(ident.lexeme);
        expr->line = line;

        if (match({TokenType::LBRACE})) {
          error("Unexpected '{' after identifier", peek().line);
          return expr;
        }
        return expr;
      }
    }

    // check if it's a function call
    if (check(TokenType::LPAREN)) {
      return functionCall();
    }

    auto expr = std::make_unique<VarExpr>(ident.lexeme);
    expr->line = line;
    return expr;
  }

  if (match({TokenType::LPAREN})) {
    auto expr = expression();
    if (!match({TokenType::RPAREN})) {
      error("Expected ')' after expression", line);
    }
    return expr;
  }

  error("Expected expression", line);
  auto expr = std::make_unique<LiteralExpr>((int64_t)0);
  expr->line = line;
  return expr;
}

} // namespace HolyLua