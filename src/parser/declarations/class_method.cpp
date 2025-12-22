#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<ClassMethod> Parser::classMethod(Visibility visibility, bool isStatic) {
  int methodLine = previous().line;

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected method name", peek().line);
    return nullptr;
  }

  Token name = advance();

  if (!match({TokenType::LPAREN})) {
    error("Expected '(' after method name", peek().line);
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

          if (declaredClasses.count(typeName) > 0 || declaredStructs.count(typeName) > 0) {
            paramType = ValueType::STRUCT;
          } else if (declaredEnums.count(typeName) > 0) {
            paramType = ValueType::ENUM;
          } else if (typeName == "number") {
            paramType = ValueType::NUMBER;
          } else if (typeName == "string") {
            paramType = ValueType::STRING;
          } else if (typeName == "bool") {
            paramType = ValueType::BOOL;
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
    if (check(TokenType::IDENTIFIER)) {
      Token typeToken = advance();
      std::string typeName = typeToken.lexeme;

      if (declaredClasses.count(typeName) > 0 || declaredStructs.count(typeName) > 0) {
        returnType = ValueType::STRUCT;
      } else if (declaredEnums.count(typeName) > 0) {
        returnType = ValueType::ENUM;
      } else if (typeName == "number") {
        returnType = ValueType::NUMBER;
      } else if (typeName == "string") {
        returnType = ValueType::STRING;
      } else if (typeName == "bool") {
        returnType = ValueType::BOOL;
      } else {
        error("Unknown return type '" + typeName + "'", typeToken.line);
        return nullptr;
      }
    } else if (match({TokenType::TYPE_NUMBER})) {
      returnType = ValueType::NUMBER;
    } else if (match({TokenType::TYPE_STRING})) {
      returnType = ValueType::STRING;
    } else if (match({TokenType::TYPE_BOOL})) {
      returnType = ValueType::BOOL;
    } else {
      error("Expected return type after ':'", peek().line);
      return nullptr;
    }

    if (match({TokenType::QUESTION})) {
      // optional return type
    }
  }

  skipNewlines();

  auto method = std::make_unique<ClassMethod>(visibility, isStatic, name.lexeme,
                                               parameters, returnType);
  method->parameterOptionals = parameterOptionals;
  method->line = methodLine;

  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      method->body.push_back(std::move(stmt));
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close method", peek().line);
    return nullptr;
  }

  skipNewlines();
  return method;
}

} // namespace HolyLua