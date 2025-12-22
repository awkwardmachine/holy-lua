#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

bool Parser::isAtEnd() { 
  return peek().type == TokenType::END_OF_FILE; 
}

Token Parser::peek() { 
  return tokens[current]; 
}

Token Parser::previous() { 
  return tokens[current - 1]; 
}

Token Parser::advance() {
  if (!isAtEnd())
    current++;
  return previous();
}

bool Parser::check(TokenType type) {
  if (isAtEnd())
    return false;
  return peek().type == type;
}

bool Parser::match(std::initializer_list<TokenType> types) {
  for (TokenType type : types) {
    if (check(type)) {
      advance();
      return true;
    }
  }
  return false;
}

void Parser::skipNewlines() {
  while (match({TokenType::NEWLINE})) {
  }
}

bool Parser::peekNextIs(TokenType type) {
  if (isAtEnd() || current + 1 >= tokens.size())
    return false;
  return tokens[current + 1].type == type;
}

ValueType Parser::parseType() {
  if (match({TokenType::TYPE_NUMBER}))
    return ValueType::NUMBER;
  if (match({TokenType::TYPE_STRING}))
    return ValueType::STRING;
  if (match({TokenType::TYPE_BOOL}))
    return ValueType::BOOL;
  
  // check if it's a struct, class, or enum type identifier
  if (check(TokenType::IDENTIFIER)) {
    Token typeToken = advance();
    std::string typeName = typeToken.lexeme;
    
    // check if it's a declared struct, class, or enum
    if (declaredStructs.count(typeName) > 0 || declaredClasses.count(typeName) > 0) {
      return ValueType::STRUCT;
    }
    
    if (declaredEnums.count(typeName) > 0) {
      return ValueType::ENUM;
    }

    error("Unknown type '" + typeName + "'", typeToken.line);
    return ValueType::INFERRED;
  }
  
  return ValueType::INFERRED;
}

} // namespace HolyLua
