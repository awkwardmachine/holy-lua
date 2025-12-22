#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<FunctionDecl> Parser::functionDeclaration() {
  int funcLine = previous().line;

  bool isGlobal = false;
  if (match({TokenType::GLOBAL})) {
    isGlobal = true;
  }

  if (!isGlobal && functionDepth == 0) {
    isGlobal = true;
  }

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected function name", peek().line);
    return nullptr;
  }

  Token name = advance();

  if (!match({TokenType::LPAREN})) {
    error("Expected '(' after function name", peek().line);
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

      if (match({TokenType::COLON})) {
        if (check(TokenType::IDENTIFIER)) {
          Token typeToken = advance();
          std::string typeName = typeToken.lexeme;
          
          if (typeName == "number") {
            paramType = ValueType::NUMBER;
          } else if (typeName == "string") {
            paramType = ValueType::STRING;
          } else if (typeName == "bool") {
            paramType = ValueType::BOOL;
          } else if (declaredStructs.count(typeName) > 0 || declaredClasses.count(typeName) > 0) {
            paramType = ValueType::STRUCT;
          } else if (declaredEnums.count(typeName) > 0) {
            paramType = ValueType::ENUM;
          } else {
            error("Unknown type '" + typeName + "'", typeToken.line);
            return nullptr;
          }
        } else if (match({TokenType::TYPE_NUMBER})) {
          paramType = ValueType::NUMBER;
        } else if (match({TokenType::TYPE_STRING})) {
          paramType = ValueType::STRING;
        } else if (match({TokenType::TYPE_BOOL})) {
          paramType = ValueType::BOOL;
        } else {
          error("Expected type after ':'", peek().line);
          return nullptr;
        }

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

  ValueType returnType = ValueType::INFERRED;
  if (match({TokenType::COLON})) {
    returnType = parseType();

    if (match({TokenType::QUESTION})) {
      // optional return type
    }
  }

  skipNewlines();

  auto funcDecl = std::make_unique<FunctionDecl>(name.lexeme, parameters,
                                                 returnType, isGlobal);
  funcDecl->parameterOptionals = parameterOptionals;
  funcDecl->line = funcLine;

  functionDepth++;

  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      funcDecl->body.push_back(std::move(stmt));
    }
  }

  functionDepth--;

  if (!match({TokenType::END})) {
    error("Expected 'end' to close function", peek().line);
    return nullptr;
  }

  skipNewlines();
  return funcDecl;
}

std::unique_ptr<FunctionCall> Parser::functionCall() {
  Token funcName = previous();
  int line = funcName.line;

  if (!match({TokenType::LPAREN})) {
    error("Expected '(' after function name", peek().line);
    return nullptr;
  }

  std::vector<std::unique_ptr<Expr>> arguments;

  if (!check(TokenType::RPAREN)) {
    do {
      arguments.push_back(expression());
    } while (match({TokenType::COMMA}));
  }

  if (!match({TokenType::RPAREN})) {
    error("Expected ')' after function arguments", peek().line);
    return nullptr;
  }

  auto call =
      std::make_unique<FunctionCall>(funcName.lexeme, std::move(arguments));
  call->line = line;
  return call;
}

} // namespace HolyLua