#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<ClassDecl> Parser::classDeclaration() {
  int classLine = previous().line;

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected class name", peek().line);
    return nullptr;
  }

  Token name = advance();
  auto classDecl = std::make_unique<ClassDecl>(name.lexeme);
  classDecl->line = classLine;

  // register this class as declared
  declaredClasses.insert(name.lexeme);

  skipNewlines();

  // parse class body
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();

    if (check(TokenType::END)) {
      break;
    }

    // check for visibility modifier
    Visibility visibility = Visibility::PRIVATE;
    bool isStatic = false;

    if (match({TokenType::PUBLIC})) {
      visibility = Visibility::PUBLIC;
      skipNewlines();
    } else if (match({TokenType::PRIVATE})) {
      visibility = Visibility::PRIVATE;
      skipNewlines();
    }

    // check for static modifier
    if (match({TokenType::STATIC})) {
      isStatic = true;
      skipNewlines();
    }

    // parse member, either field or method
    if (match({TokenType::FUNCTION})) {
      auto method = classMethod(visibility, isStatic);
      if (!method) {
        return nullptr;
      }

      // check if constructor
      if (method->name == "__init") {
        if (classDecl->constructor) {
          error("Class can only have one __init constructor", method->line);
          return nullptr;
        }
        if (isStatic) {
          error("Constructor __init cannot be static", method->line);
          return nullptr;
        }
        classDecl->constructor = std::move(method);
      } else {
        classDecl->methods.push_back(std::move(*method));
      }
    } else {
      // parse field
      auto field = classField(visibility, isStatic);
      if (!field) {
        return nullptr;
      }
      classDecl->fields.push_back(std::move(*field));
    }

    skipNewlines();
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close class", peek().line);
    return nullptr;
  }

  skipNewlines();
  return classDecl;
}

std::unique_ptr<ClassInstantiation> Parser::classInstantiation() {
  Token className = previous();
  int line = className.line;

  if (!match({TokenType::LPAREN})) {
    error("Expected '(' after class name", peek().line);
    return nullptr;
  }

  std::vector<std::unique_ptr<Expr>> arguments;

  if (!check(TokenType::RPAREN)) {
    do {
      arguments.push_back(expression());
    } while (match({TokenType::COMMA}));
  }

  if (!match({TokenType::RPAREN})) {
    error("Expected ')' after arguments", peek().line);
    return nullptr;
  }

  auto instantiation = std::make_unique<ClassInstantiation>(className.lexeme,
                                                             std::move(arguments));
  instantiation->line = line;
  return instantiation;
}

} // namespace HolyLua
