#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<StructDecl> Parser::structDeclaration() {
  int structLine = previous().line;

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected struct name", peek().line);
    return nullptr;
  }

  Token name = advance();
  auto structDecl = std::make_unique<StructDecl>(name.lexeme);
  structDecl->line = structLine;

  // register this struct as declared
  declaredStructs.insert(name.lexeme);

  skipNewlines();

  // parse struct fields
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();

    if (check(TokenType::END)) {
      break;
    }

    // parse field name
    if (!check(TokenType::IDENTIFIER)) {
      error("Expected field name", peek().line);
      return nullptr;
    }

    Token fieldName = advance();
    ValueType fieldType = ValueType::INFERRED;
    std::string structTypeName = "";
    bool isOptional = false;
    bool hasDefault = false;
    std::variant<int64_t, double, std::string, bool, std::nullptr_t>
        defaultValue = nullptr;

    // parse type annotation
    if (match({TokenType::COLON})) {
      if (check(TokenType::IDENTIFIER)) {
        Token typeToken = advance();
        std::string typeName = typeToken.lexeme;

        if (declaredStructs.count(typeName) > 0) {
          fieldType = ValueType::STRUCT;
          structTypeName = typeName;
        }
        else if (declaredEnums.count(typeName) > 0) {
          fieldType = ValueType::ENUM;
          structTypeName = typeName;
        }
        else if (declaredClasses.count(typeName) > 0) {
          fieldType = ValueType::STRUCT;
          structTypeName = typeName;
        }
        else if (typeName == "number") {
          fieldType = ValueType::NUMBER;
        } else if (typeName == "string") {
          fieldType = ValueType::STRING;
        } else if (typeName == "bool") {
          fieldType = ValueType::BOOL;
        } else {
          error("Unknown type '" + typeName + "'", typeToken.line);
          return nullptr;
        }
      }
      else {
        fieldType = parseType();
      }

      if (match({TokenType::QUESTION})) {
        isOptional = true;
      }
    }

    // parse default value
    if (match({TokenType::ASSIGN})) {
      hasDefault = true;

      // parse literal default value
      if (match({TokenType::NUMBER})) {
        const auto &lit = previous().literal;
        if (std::holds_alternative<int64_t>(lit)) {
          defaultValue = std::get<int64_t>(lit);
        } else if (std::holds_alternative<double>(lit)) {
          defaultValue = std::get<double>(lit);
        }
      } else if (match({TokenType::STRING})) {
        defaultValue = std::get<std::string>(previous().literal);
      } else if (match({TokenType::TRUE})) {
        defaultValue = true;
      } else if (match({TokenType::FALSE})) {
        defaultValue = false;
      } else if (match({TokenType::NIL})) {
        defaultValue = nullptr;
      } else {
        error("Default value must be a literal", peek().line);
        return nullptr;
      }
    }

    if (match({TokenType::COMMA})) {
      skipNewlines();
    }
    else {
      skipNewlines();
    }

    // create field with struct type info if applicable
    StructField field(fieldName.lexeme, fieldType, isOptional, hasDefault);
    if (!structTypeName.empty()) {
      field.structTypeName = structTypeName;
    }
    if (hasDefault) {
      field.defaultValue = defaultValue;
    }
    structDecl->fields.push_back(std::move(field));
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close struct", peek().line);
    return nullptr;
  }

  skipNewlines();
  return structDecl;
}

std::unique_ptr<StructConstructor> Parser::structConstructor() {
  int line = previous().line;
  Token structName = previous();

  auto constructor = std::make_unique<StructConstructor>(structName.lexeme);
  constructor->line = line;

  if (!match({TokenType::LBRACE})) {
    error("Expected '{' after struct name", peek().line);
    return nullptr;
  }

  skipNewlines();

  // check if it's empty constructor with defaults
  if (match({TokenType::RBRACE})) {
    constructor->useDefaults = true;
    return constructor;
  }

  bool isNamed = false;
  size_t saved = current;
  
  while (true) {
    while (current < tokens.size() && tokens[current].type == TokenType::NEWLINE) {
      current++;
    }
    
    // check if identifier followed by '=' or ':'
    if (current < tokens.size() && tokens[current].type == TokenType::IDENTIFIER) {
      size_t lookahead = current + 1;
      
      while (lookahead < tokens.size() && tokens[lookahead].type == TokenType::NEWLINE) {
        lookahead++;
      }
      
      // check for either '=' or ':' as assignment operators for struct fields
      if (lookahead < tokens.size() && 
          (tokens[lookahead].type == TokenType::ASSIGN || 
           tokens[lookahead].type == TokenType::COLON)) {
        isNamed = true;
        break;
      }
    }
    
    if (current >= tokens.size() || 
        tokens[current].type == TokenType::RBRACE || 
        tokens[current].type == TokenType::COMMA ||
        tokens[current].type == TokenType::NUMBER ||
        tokens[current].type == TokenType::STRING ||
        tokens[current].type == TokenType::TRUE ||
        tokens[current].type == TokenType::FALSE ||
        tokens[current].type == TokenType::NIL) {
      break;
    }

    current++;
  }
  
  current = saved;

  if (isNamed) {
    // parse named arguments
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
      skipNewlines();

      if (check(TokenType::RBRACE)) {
        break;
      }

      if (!check(TokenType::IDENTIFIER)) {
        error("Expected field name in struct constructor", peek().line);
        return nullptr;
      }

      Token fieldName = advance();

      // accept either '=' or ':' as assignment operator
      if (!match({TokenType::ASSIGN, TokenType::COLON})) {
        error("Expected '=' or ':' after field name", peek().line);
        return nullptr;
      }

      auto arg = expression();
      constructor->namedArgs.emplace_back(fieldName.lexeme, std::move(arg));

      skipNewlines();
      
      // check for comma or closing brace
      if (check(TokenType::RBRACE)) {
        break;
      }
      
      if (!match({TokenType::COMMA})) {
        if (!check(TokenType::RBRACE)) {
          error("Expected ',' or '}' after field assignment", peek().line);
          return nullptr;
        }
      }
    }

    if (!match({TokenType::RBRACE})) {
      error("Expected '}' after struct constructor", peek().line);
      return nullptr;
    }
  } else {
    // parse positional arguments
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
      skipNewlines();

      if (check(TokenType::RBRACE)) {
        break;
      }

      auto arg = expression();
      constructor->positionalArgs.push_back(std::move(arg));

      skipNewlines();

      if (check(TokenType::RBRACE)) {
        break;
      }
      
      if (!match({TokenType::COMMA})) {
        if (!check(TokenType::RBRACE)) {
          error("Expected ',' or '}' after argument", peek().line);
          return nullptr;
        }
      }
    }

    if (!match({TokenType::RBRACE})) {
      error("Expected '}' after struct constructor arguments", peek().line);
      return nullptr;
    }
  }

  return constructor;
}

} // namespace HolyLua