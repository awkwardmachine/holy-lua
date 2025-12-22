#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<Expr> Parser::expression() { 
  return logicalOr(); 
}

std::unique_ptr<Expr> Parser::logicalOr() {
  auto expr = logicalAnd();

  while (match({TokenType::OR})) {
    Token op = previous();
    int line = op.line;
    auto right = logicalAnd();

    auto binExpr = std::make_unique<BinaryExpr>(std::move(expr), BinaryOp::OR,
                                                std::move(right));
    binExpr->line = line;
    expr = std::move(binExpr);
  }

  return expr;
}

std::unique_ptr<Expr> Parser::logicalAnd() {
  auto expr = nilCoalescing();

  while (match({TokenType::AND})) {
    Token op = previous();
    int line = op.line;
    auto right = nilCoalescing();

    auto binExpr = std::make_unique<BinaryExpr>(std::move(expr), BinaryOp::AND,
                                                std::move(right));
    binExpr->line = line;
    expr = std::move(binExpr);
  }

  return expr;
}

std::unique_ptr<Expr> Parser::nilCoalescing() {
  auto expr = concat();

  while (match({TokenType::DOUBLE_QUESTION})) {
    Token op = previous();
    int line = op.line;
    auto right = concat();

    auto binExpr = std::make_unique<BinaryExpr>(
        std::move(expr), BinaryOp::NIL_COALESCE, std::move(right));
    binExpr->line = line;
    expr = std::move(binExpr);
  }

  return expr;
}

std::unique_ptr<Expr> Parser::concat() {
  auto expr = comparison();

  while (match({TokenType::CONCAT})) {
    Token op = previous();
    int line = op.line;
    auto right = comparison();

    auto binExpr = std::make_unique<BinaryExpr>(
        std::move(expr), BinaryOp::CONCAT, std::move(right));
    binExpr->line = line;
    expr = std::move(binExpr);
  }

  return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
  auto expr = additive();

  while (match({TokenType::EQUAL, TokenType::NOT_EQUAL, TokenType::LESS,
                TokenType::LESS_EQUAL, TokenType::GREATER,
                TokenType::GREATER_EQUAL})) {
    Token op = previous();
    int line = op.line;
    auto right = additive();

    BinaryOp binOp;
    if (op.type == TokenType::EQUAL)
      binOp = BinaryOp::EQUAL;
    else if (op.type == TokenType::NOT_EQUAL)
      binOp = BinaryOp::NOT_EQUAL;
    else if (op.type == TokenType::LESS)
      binOp = BinaryOp::LESS;
    else if (op.type == TokenType::LESS_EQUAL)
      binOp = BinaryOp::LESS_EQUAL;
    else if (op.type == TokenType::GREATER)
      binOp = BinaryOp::GREATER;
    else
      binOp = BinaryOp::GREATER_EQUAL;

    auto binExpr =
        std::make_unique<BinaryExpr>(std::move(expr), binOp, std::move(right));
    binExpr->line = line;
    expr = std::move(binExpr);
  }

  return expr;
}

std::unique_ptr<Expr> Parser::additive() {
  auto expr = multiplicative();

  while (match({TokenType::PLUS, TokenType::MINUS})) {
    Token op = previous();
    int line = op.line;
    auto right = multiplicative();
    BinaryOp binOp =
        (op.type == TokenType::PLUS) ? BinaryOp::ADD : BinaryOp::SUBTRACT;
    auto binExpr =
        std::make_unique<BinaryExpr>(std::move(expr), binOp, std::move(right));
    binExpr->line = line;
    expr = std::move(binExpr);
  }

  return expr;
}

std::unique_ptr<Expr> Parser::multiplicative() {
  auto expr = power();

  while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT,
                TokenType::DOUBLE_SLASH})) {
    Token op = previous();
    int line = op.line;
    auto right = power();
    BinaryOp binOp;
    if (op.type == TokenType::STAR)
      binOp = BinaryOp::MULTIPLY;
    else if (op.type == TokenType::SLASH)
      binOp = BinaryOp::DIVIDE;
    else if (op.type == TokenType::PERCENT)
      binOp = BinaryOp::MODULO;
    else
      binOp = BinaryOp::FLOOR_DIVIDE;
    auto binExpr =
        std::make_unique<BinaryExpr>(std::move(expr), binOp, std::move(right));
    binExpr->line = line;
    expr = std::move(binExpr);
  }

  return expr;
}

std::unique_ptr<Expr> Parser::power() {
  auto expr = unary();

  if (match({TokenType::DOUBLE_STAR})) {
    Token op = previous();
    int line = op.line;
    // right-associative, recursively call power()
    auto right = power();
    auto binExpr = std::make_unique<BinaryExpr>(
        std::move(expr), BinaryOp::POWER, std::move(right));
    binExpr->line = line;
    return binExpr;
  }

  return expr;
}

std::unique_ptr<Expr> Parser::unary() {
  if (match({TokenType::MINUS})) {
    int line = previous().line;
    auto operand = unary();
    auto expr =
        std::make_unique<UnaryExpr>(UnaryOp::NEGATE, std::move(operand));
    expr->line = line;
    return expr;
  }

  if (match({TokenType::NOT})) {
    int line = previous().line;
    auto operand = unary();
    auto expr = std::make_unique<UnaryExpr>(UnaryOp::NOT, std::move(operand));
    expr->line = line;
    return expr;
  }

  return postfix();
}

} // namespace HolyLua
