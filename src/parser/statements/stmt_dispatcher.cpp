#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<ASTNode> Parser::statement() {
  if (match({TokenType::INLINE})) {
    return inlineCStatement();
  }

  if (match({TokenType::WHILE})) {
    return whileStatement();
  }

  if (match({TokenType::REPEAT})) {
    return repeatStatement();
  }

  if (match({TokenType::IF})) {
    return ifStatement();
  }

  if (match({TokenType::FOR})) {
    return forStatement();
  }

  if (match({TokenType::PRINT})) {
    return printStatement();
  }

  if (match({TokenType::RETURN})) {
    return returnStatement();
  }

  if (match({TokenType::FUNCTION})) {
    return functionDeclaration();
  }

  if (match({TokenType::STRUCT})) {
    return structDeclaration();
  }

  if (match({TokenType::CLASS})) {
    return classDeclaration();
  }

  if (match({TokenType::ENUM})) {
    return enumDeclaration();
  }

  return declaration();
}

} // namespace HolyLua
