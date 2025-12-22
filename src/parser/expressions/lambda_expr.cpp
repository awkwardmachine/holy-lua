#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<LambdaExpr> Parser::lambdaExpression(int line) {
  // parameters
  if (!match({TokenType::LPAREN})) {
    error("Expected '(' after function keyword", peek().line);
    return nullptr;
  }

  std::vector<std::pair<std::string, ValueType>> parameters;
  std::vector<bool> parameterOptionals;

  if (!check(TokenType::RPAREN)) {
    do {
      if (!check(TokenType::IDENTIFIER)) {
        error("Expected parameter name", peek().line);
        return nullptr;
      }

      Token paramName = advance();
      ValueType paramType = ValueType::INFERRED;
      bool isOptional = false;

      // optional type annotation
      if (match({TokenType::COLON})) {
        paramType = parseType();

        // check for optional marker '?' after the type
        if (match({TokenType::QUESTION})) {
          isOptional = true;
        }
      }

      parameters.emplace_back(paramName.lexeme, paramType);
      parameterOptionals.push_back(isOptional);
    } while (match({TokenType::COMMA}));
  }

  if (!match({TokenType::RPAREN})) {
    error("Expected ')' after parameters", peek().line);
    return nullptr;
  }

  // optional return type annotation
  ValueType returnType = ValueType::INFERRED;
  if (match({TokenType::COLON})) {
    returnType = parseType();
  }

  skipNewlines();

  auto lambda = std::make_unique<LambdaExpr>(parameters, returnType);
  lambda->parameterOptionals = parameterOptionals;
  lambda->line = line;

  // increment depth when entering function body
  functionDepth++;

  // function body
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      lambda->body.push_back(std::move(stmt));
    }
  }

  // decrement depth when leaving function body
  functionDepth--;

  if (!match({TokenType::END})) {
    error("Expected 'end' to close anonymous function", peek().line);
    return nullptr;
  }

  skipNewlines();
  return lambda;
}

} // namespace HolyLua
