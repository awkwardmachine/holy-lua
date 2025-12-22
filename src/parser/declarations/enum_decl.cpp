#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<ASTNode> Parser::enumDeclaration() {
  int line = previous().line;
  
  if (!check(TokenType::IDENTIFIER)) {
    error("Expected enum name", line);
    return nullptr;
  }
  
  Token nameToken = advance();
  std::string enumName = nameToken.lexeme;
  
  if (declaredEnums.count(enumName) || declaredStructs.count(enumName) || 
      declaredClasses.count(enumName)) {
    error("Type '" + enumName + "' already declared", line);
    return nullptr;
  }
  
  auto enumDecl = std::make_unique<EnumDecl>(enumName);
  enumDecl->line = line;
  
  skipNewlines();
  
  std::vector<std::string> values;
  
  // parse enum values until 'end'
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    
    if (check(TokenType::END)) {
      break;
    }
    
    if (!check(TokenType::IDENTIFIER)) {
      error("Expected enum value name", peek().line);
      advance();
      continue;
    }
    
    Token valueToken = advance();
    std::string valueName = valueToken.lexeme;
    
    values.push_back(valueName);
    
    skipNewlines();
  }
  
  if (!match({TokenType::END})) {
    error("Expected 'end' after enum declaration", line);
    return nullptr;
  }
  
  enumDecl->values = values;
  declaredEnums.insert(enumName);
  enumValues[enumName] = values;
  
  skipNewlines();
  
  return enumDecl;
}

} // namespace HolyLua