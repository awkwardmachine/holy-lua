#include "../../../include/parser.h"
#include "../../../include/token.h"

namespace HolyLua {

std::unique_ptr<ASTNode> Parser::declaration() {
  size_t savedPos = current;
  
  // check for simple identifier assignment
  if (check(TokenType::IDENTIFIER)) {
    Token name = peek();
    advance();

    // check for compound assignment operators
    if (match({TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN,
               TokenType::STAR_ASSIGN, TokenType::SLASH_ASSIGN,
               TokenType::PERCENT_ASSIGN, TokenType::DOUBLE_STAR_ASSIGN,
               TokenType::DOUBLE_SLASH_ASSIGN})) {
      Token op = previous();
      auto expr = expression();
      skipNewlines();

      BinaryOp binOp;
      if (op.type == TokenType::PLUS_ASSIGN)
        binOp = BinaryOp::ADD;
      else if (op.type == TokenType::MINUS_ASSIGN)
        binOp = BinaryOp::SUBTRACT;
      else if (op.type == TokenType::STAR_ASSIGN)
        binOp = BinaryOp::MULTIPLY;
      else if (op.type == TokenType::SLASH_ASSIGN)
        binOp = BinaryOp::DIVIDE;
      else if (op.type == TokenType::PERCENT_ASSIGN)
        binOp = BinaryOp::MODULO;
      else if (op.type == TokenType::DOUBLE_STAR_ASSIGN)
        binOp = BinaryOp::POWER;
      else
        binOp = BinaryOp::FLOOR_DIVIDE;

      auto assign =
          std::make_unique<Assignment>(name.lexeme, std::move(expr), binOp);
      assign->line = name.line;
      return assign;
    } else if (match({TokenType::ASSIGN})) {
      auto expr = expression();
      skipNewlines();
      auto assign = std::make_unique<Assignment>(name.lexeme, std::move(expr));
      assign->line = name.line;
      return assign;
    } else if (check(TokenType::LPAREN)) {
      auto call = functionCall();
      skipNewlines();
      return call;
    }
    
    // check if it's a field access that will be assigned
    if (check(TokenType::DOT)) {
      current = savedPos;

      auto leftExpr = postfix();
      
      // now check if there's an assignment
      if (auto *fieldAccess = dynamic_cast<FieldAccessExpr*>(leftExpr.get())) {
        if (match({TokenType::ASSIGN})) {
          int line = previous().line;
          auto value = expression();
          skipNewlines();
          
          auto assignment = std::make_unique<FieldAssignment>(
              std::move(fieldAccess->object),
              fieldAccess->fieldName,
              std::move(value)
          );
          assignment->line = line;
          return assignment;
        } else if (match({TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN,
                          TokenType::STAR_ASSIGN, TokenType::SLASH_ASSIGN,
                          TokenType::PERCENT_ASSIGN, TokenType::DOUBLE_STAR_ASSIGN,
                          TokenType::DOUBLE_SLASH_ASSIGN})) {
          // compound field assignment
          Token op = previous();
          int line = op.line;
          auto value = expression();
          skipNewlines();
          
          BinaryOp binOp;
          if (op.type == TokenType::PLUS_ASSIGN)
            binOp = BinaryOp::ADD;
          else if (op.type == TokenType::MINUS_ASSIGN)
            binOp = BinaryOp::SUBTRACT;
          else if (op.type == TokenType::STAR_ASSIGN)
            binOp = BinaryOp::MULTIPLY;
          else if (op.type == TokenType::SLASH_ASSIGN)
            binOp = BinaryOp::DIVIDE;
          else if (op.type == TokenType::PERCENT_ASSIGN)
            binOp = BinaryOp::MODULO;
          else if (op.type == TokenType::DOUBLE_STAR_ASSIGN)
            binOp = BinaryOp::POWER;
          else
            binOp = BinaryOp::FLOOR_DIVIDE;
          
          auto assignment = std::make_unique<FieldAssignment>(
              std::move(fieldAccess->object),
              fieldAccess->fieldName,
              std::move(value),
              binOp
          );
          assignment->line = line;
          return assignment;
        }
      }
      
      skipNewlines();
      return leftExpr;
    }
    
    current--;
  }
  
  if (check(TokenType::SELF)) {
    savedPos = current;
    auto leftExpr = postfix();
    
    if (auto *fieldAccess = dynamic_cast<FieldAccessExpr*>(leftExpr.get())) {
      if (match({TokenType::ASSIGN})) {
        int line = previous().line;
        auto value = expression();
        skipNewlines();
        
        auto assignment = std::make_unique<FieldAssignment>(
            std::move(fieldAccess->object),
            fieldAccess->fieldName,
            std::move(value)
        );
        assignment->line = line;
        return assignment;
      } else if (match({TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN,
                        TokenType::STAR_ASSIGN, TokenType::SLASH_ASSIGN,
                        TokenType::PERCENT_ASSIGN, TokenType::DOUBLE_STAR_ASSIGN,
                        TokenType::DOUBLE_SLASH_ASSIGN})) {
        Token op = previous();
        int line = op.line;
        auto value = expression();
        skipNewlines();
        
        BinaryOp binOp;
        if (op.type == TokenType::PLUS_ASSIGN)
          binOp = BinaryOp::ADD;
        else if (op.type == TokenType::MINUS_ASSIGN)
          binOp = BinaryOp::SUBTRACT;
        else if (op.type == TokenType::STAR_ASSIGN)
          binOp = BinaryOp::MULTIPLY;
        else if (op.type == TokenType::SLASH_ASSIGN)
          binOp = BinaryOp::DIVIDE;
        else if (op.type == TokenType::PERCENT_ASSIGN)
          binOp = BinaryOp::MODULO;
        else if (op.type == TokenType::DOUBLE_STAR_ASSIGN)
          binOp = BinaryOp::POWER;
        else
          binOp = BinaryOp::FLOOR_DIVIDE;
        
        auto assignment = std::make_unique<FieldAssignment>(
            std::move(fieldAccess->object),
            fieldAccess->fieldName,
            std::move(value),
            binOp
        );
        assignment->line = line;
        return assignment;
      }
    }
    
    current = savedPos;
  }

  // check if next token is a declaration keyword
  if (peek().type == TokenType::LOCAL || peek().type == TokenType::GLOBAL ||
      peek().type == TokenType::CONST) {
    return varDeclaration();
  }

  auto expr = expression();
  skipNewlines();
  return expr;
}

std::unique_ptr<VarDecl> Parser::varDeclaration() {
  Token firstKeyword = peek();

  bool isLocal = false;
  bool isGlobal = false;
  bool isConst = false;

  while (match({TokenType::LOCAL, TokenType::GLOBAL, TokenType::CONST})) {
    Token modifier = previous();
    if (modifier.type == TokenType::LOCAL) {
      isLocal = true;
    } else if (modifier.type == TokenType::GLOBAL) {
      isGlobal = true;
    } else if (modifier.type == TokenType::CONST) {
      isConst = true;
    }
  }

  int declLine = firstKeyword.line;

  if (!isLocal && !isGlobal && !isConst) {
    error("Expected declaration keyword (local/global/const)", declLine);
    return nullptr;
  }

  if (isLocal && isGlobal) {
    error("Variable cannot be both local and global", declLine);
    return nullptr;
  }

  bool isGlobalVar = isGlobal;

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected identifier after declaration keywords", declLine);
    return nullptr;
  }

  Token name = advance();

  ValueType type = ValueType::INFERRED;
  bool isOptional = false;
  std::string typeName = "";

  // optional type annotation
  if (match({TokenType::COLON})) {
    // check if type is an identifier
    if (check(TokenType::IDENTIFIER)) {
      Token typeToken = advance();
      std::string identifier = typeToken.lexeme;
      
      if (declaredEnums.count(identifier) > 0) {
        type = ValueType::ENUM;
        typeName = identifier;
      }
      else if (declaredStructs.count(identifier) > 0 || declaredClasses.count(identifier) > 0) {
        type = ValueType::STRUCT;
        typeName = identifier;
      }
      else if (identifier == "number") {
        type = ValueType::NUMBER;
        typeName = "number";
      } else if (identifier == "string") {
        type = ValueType::STRING;
        typeName = "string";
      } else if (identifier == "bool") {
        type = ValueType::BOOL;
        typeName = "bool";
      } else {
        error("Unknown type '" + identifier + "'", typeToken.line);
        return nullptr;
      }
    }
    else if (match({TokenType::TYPE_NUMBER})) {
      type = ValueType::NUMBER;
      typeName = "number";
    } else if (match({TokenType::TYPE_STRING})) {
      type = ValueType::STRING;
      typeName = "string";
    } else if (match({TokenType::TYPE_BOOL})) {
      type = ValueType::BOOL;
      typeName = "bool";
    } else {
      error("Expected type after ':'", peek().line);
      return nullptr;
    }

    // check for optional type marker (?)
    if (match({TokenType::QUESTION})) {
      isOptional = true;
    }
  }

  auto decl = std::make_unique<VarDecl>(isGlobalVar, isConst, name.lexeme, type, isOptional);
  decl->line = declLine;
  decl->typeName = typeName;

  // optional initializer
  if (match({TokenType::ASSIGN})) {
    decl->value = expression();
    decl->hasValue = true;
  }

  skipNewlines();

  return decl;
}

} // namespace HolyLua
