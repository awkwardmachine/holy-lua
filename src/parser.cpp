#include "parser.h"
#include "token.h"
#include <iostream>
#include <sstream>
#include <vector>

namespace HolyLua {

Parser::Parser(const std::vector<Token> &tokens, const std::string &source)
    : tokens(tokens), source(source) {
  initSourceLines();
}

void Parser::initSourceLines() {
  std::stringstream ss(source);
  std::string line;
  while (std::getline(ss, line)) {
    sourceLines.push_back(line);
  }
}

void Parser::error(const std::string &msg, int line) {
  std::cerr << "\033[1;31mError:\033[0m " << msg << "\n";
  showErrorContext(line);
}

void Parser::showErrorContext(int line) {
  if (line < 1 || line > (int)sourceLines.size())
    return;

  int lineIdx = line - 1;

  if (lineIdx > 0) {
    std::cerr << "  " << (lineIdx) << " | " << sourceLines[lineIdx - 1] << "\n";
  }

  std::cerr << "\033[1;33m> " << line << " | " << sourceLines[lineIdx]
            << "\033[0m\n";

  if (lineIdx < (int)sourceLines.size() - 1) {
    std::cerr << "  " << (lineIdx + 2) << " | " << sourceLines[lineIdx + 1]
              << "\n";
  }
  std::cerr << "\n";
}

Program Parser::parse() {
  Program program;

  while (!isAtEnd()) {
    skipNewlines();
    if (!isAtEnd() && peek().type != TokenType::END_OF_FILE) {
      auto stmt = statement();
      if (stmt) {
        program.statements.push_back(std::move(stmt));
      }
    }
  }

  return program;
}

bool Parser::isAtEnd() { return peek().type == TokenType::END_OF_FILE; }

Token Parser::peek() { return tokens[current]; }

Token Parser::previous() { return tokens[current - 1]; }

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

std::unique_ptr<InlineCStmt> Parser::inlineCStatement() {
  int line = previous().line;

  if (!match({TokenType::C})) {
    error("Expected 'C' after 'inline'", peek().line);
    return nullptr;
  }

  skipNewlines();

  std::string cCode;

  while (!isAtEnd() && peek().type != TokenType::END) {
    Token token = advance();
    if (token.type == TokenType::NEWLINE) {
      cCode += "\n";
      continue;
    }

    if (token.type == TokenType::STRING) {
      cCode += "\"" + std::get<std::string>(token.literal) + "\"";
    } else {
      cCode += token.lexeme;
    }

    if (!isAtEnd() && peek().type != TokenType::END &&
        peek().type != TokenType::NEWLINE) {
      cCode += " ";
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close inline C block", peek().line);
    return nullptr;
  }

  while (!cCode.empty() && (cCode.back() == ' ' || cCode.back() == '\n')) {
    cCode.pop_back();
  }

  auto inlineC = std::make_unique<InlineCStmt>(cCode);
  inlineC->line = line;
  skipNewlines();
  return inlineC;
}

std::unique_ptr<RepeatStmt> Parser::repeatStatement() {
  int repeatLine = previous().line;
  auto repeatStmt = std::make_unique<RepeatStmt>(nullptr);
  repeatStmt->line = repeatLine;

  skipNewlines();

  // parse repeat loop body
  while (!check(TokenType::UNTIL) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::UNTIL))
      break;

    auto stmt = statement();
    if (stmt) {
      repeatStmt->body.push_back(std::move(stmt));
    }
  }

  if (!match({TokenType::UNTIL})) {
    error("Expected 'until' after repeat body", peek().line);
    return nullptr;
  }

  // parse the until condition
  repeatStmt->condition = expression();

  skipNewlines();
  return repeatStmt;
}

std::unique_ptr<ForStmt> Parser::forStatement() {
  int forLine = previous().line;

  // parse 'for local x = start, end, step'
  if (!match({TokenType::LOCAL})) {
    error("Expected 'local' in for loop declaration", peek().line);
    return nullptr;
  }

  if (!check(TokenType::IDENTIFIER)) {
    error("Expected variable name in for loop", peek().line);
    return nullptr;
  }

  Token varName = advance();

  if (!match({TokenType::ASSIGN})) {
    error("Expected '=' after for loop variable", peek().line);
    return nullptr;
  }

  auto start = expression();

  if (!match({TokenType::COMMA})) {
    error("Expected ',' after start value", peek().line);
    return nullptr;
  }

  auto end = expression();

  std::unique_ptr<Expr> step = nullptr;
  if (match({TokenType::COMMA})) {
    step = expression();
  }

  skipNewlines();

  auto forStmt = std::make_unique<ForStmt>(varName.lexeme, std::move(start),
                                           std::move(end), std::move(step));
  forStmt->line = forLine;

  // parse for loop body
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      forStmt->body.push_back(std::move(stmt));
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close for loop", peek().line);
    return nullptr;
  }

  skipNewlines();
  return forStmt;
}

std::unique_ptr<WhileStmt> Parser::whileStatement() {
  int whileLine = previous().line;

  auto condition = expression();

  skipNewlines();

  auto whileStmt = std::make_unique<WhileStmt>(std::move(condition));
  whileStmt->line = whileLine;

  // parse while loop body
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      whileStmt->body.push_back(std::move(stmt));
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close while statement", peek().line);
    return nullptr;
  }

  skipNewlines();
  return whileStmt;
}

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

  return declaration();
}

std::unique_ptr<ReturnStmt> Parser::returnStatement() {
  int returnLine = previous().line;
  auto returnStmt = std::make_unique<ReturnStmt>();

  if (!check(TokenType::NEWLINE) && !check(TokenType::END) && !isAtEnd()) {
    returnStmt->value = expression();
  }

  skipNewlines();
  returnStmt->line = returnLine;
  return returnStmt;
}

std::unique_ptr<FunctionDecl> Parser::functionDeclaration() {
  int funcLine = previous().line;

  // optional global modifier
  bool isGlobal = false;
  if (match({TokenType::GLOBAL})) {
    isGlobal = true;
  }

  // if no explicit global keyword, top-level functions are global by default
  if (!isGlobal && functionDepth == 0) {
    isGlobal = true;
  }

  // function name
  if (!check(TokenType::IDENTIFIER)) {
    error("Expected function name", peek().line);
    return nullptr;
  }

  Token name = advance();

  // parameters
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

  auto funcDecl = std::make_unique<FunctionDecl>(name.lexeme, parameters,
                                                 returnType, isGlobal);
  funcDecl->parameterOptionals = parameterOptionals;
  funcDecl->line = funcLine;

  // increment depth when entering function body
  functionDepth++;

  // function body
  while (!check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::END))
      break;

    auto stmt = statement();
    if (stmt) {
      funcDecl->body.push_back(std::move(stmt));
    }
  }

  // decrement depth when leaving function body
  functionDepth--;

  if (!match({TokenType::END})) {
    error("Expected 'end' to close function", peek().line);
    return nullptr;
  }

  skipNewlines();
  return funcDecl;
}

std::unique_ptr<IfStmt> Parser::ifStatement() {
  int ifLine = previous().line;

  auto condition = expression();

  if (!match({TokenType::THEN})) {
    error("Expected 'then' after if condition", peek().line);
    return nullptr;
  }

  skipNewlines();

  auto ifStmt = std::make_unique<IfStmt>(std::move(condition));
  ifStmt->line = ifLine;

  // parse then block
  while (!check(TokenType::ELSE) && !check(TokenType::END) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::ELSE) || check(TokenType::END))
      break;
    auto stmt = statement();
    if (stmt) {
      ifStmt->thenBlock.push_back(std::move(stmt));
    }
  }

  // parse optional else block
  if (match({TokenType::ELSE})) {
    skipNewlines();
    while (!check(TokenType::END) && !isAtEnd()) {
      skipNewlines();
      if (check(TokenType::END))
        break;
      auto stmt = statement();
      if (stmt) {
        ifStmt->elseBlock.push_back(std::move(stmt));
      }
    }
  }

  if (!match({TokenType::END})) {
    error("Expected 'end' to close if statement", peek().line);
    return nullptr;
  }

  skipNewlines();

  return ifStmt;
}

std::unique_ptr<ASTNode> Parser::declaration() {
  // check for assignment first
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
      // this is a function call as a statement (e.g., test())
      auto call = functionCall();
      skipNewlines();
      return call;
    }
    current--;
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

  // Increment depth when entering function body
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

  // Decrement depth when leaving function body
  functionDepth--;

  if (!match({TokenType::END})) {
    error("Expected 'end' to close anonymous function", peek().line);
    return nullptr;
  }

  skipNewlines();
  return lambda;
}

std::unique_ptr<PrintStmt> Parser::printStatement() {
  int printLine = previous().line;

  if (!match({TokenType::LPAREN})) {
    error("Expected '(' after 'print'", printLine);
    return nullptr;
  }

  std::vector<PrintArg> args;

  if (!check(TokenType::RPAREN)) {
    do {
      auto expr = expression();
      if (auto *varExpr = dynamic_cast<VarExpr *>(expr.get())) {
        args.emplace_back(varExpr->name);
      } else {
        args.emplace_back(std::move(expr));
      }
    } while (match({TokenType::COMMA}));
  }

  if (!match({TokenType::RPAREN})) {
    error("Expected ')' after print arguments", peek().line);
    return nullptr;
  }

  skipNewlines();

  auto stmt = std::make_unique<PrintStmt>(std::move(args));
  stmt->line = printLine;
  return stmt;
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
      
      // check if it's a declared struct
      if (declaredStructs.count(identifier) > 0) {
        type = ValueType::STRUCT;
        typeName = identifier;
      }
      // check if it's a built-in type
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
    // otherwise check for token-based type keywords
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

ValueType Parser::parseType() {
  if (match({TokenType::TYPE_NUMBER}))
    return ValueType::NUMBER;
  if (match({TokenType::TYPE_STRING}))
    return ValueType::STRING;
  if (match({TokenType::TYPE_BOOL}))
    return ValueType::BOOL;
  
  // check if it's a struct type identifier
  if (check(TokenType::IDENTIFIER)) {
    Token typeToken = advance();
    std::string typeName = typeToken.lexeme;
    
    // check if it's a declared struct
    if (declaredStructs.count(typeName) > 0) {
      return ValueType::STRUCT;
    }
    
    // if not a struct, it's an error
    error("Unknown type '" + typeName + "'", typeToken.line);
    return ValueType::INFERRED;
  }
  
  return ValueType::INFERRED;
}

std::unique_ptr<Expr> Parser::expression() { return logicalOr(); }

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

std::unique_ptr<Expr> Parser::postfix() {
  auto expr = primary();

  // handle field access
  while (match({TokenType::DOT})) {
    if (!check(TokenType::IDENTIFIER)) {
      error("Expected field name after '.'", peek().line);
      return expr;
    }

    Token field = advance();
    auto fieldAccess =
        std::make_unique<FieldAccessExpr>(std::move(expr), field.lexeme);
    fieldAccess->line = field.line;
    expr = std::move(fieldAccess);
  }

  // check for force unwrap operator
  if (match({TokenType::BANG})) {
    int line = previous().line;
    auto unwrap = std::make_unique<ForceUnwrapExpr>(std::move(expr));
    unwrap->line = line;
    return unwrap;
  }

  return expr;
}

std::unique_ptr<Expr> Parser::primary() {
  int line = peek().line;

  if (match({TokenType::NUMBER})) {
    const auto &lit = previous().literal;
    auto expr = std::unique_ptr<LiteralExpr>(nullptr);
    if (std::holds_alternative<int64_t>(lit)) {
      expr = std::make_unique<LiteralExpr>(std::get<int64_t>(lit));
    } else if (std::holds_alternative<double>(lit)) {
      expr = std::make_unique<LiteralExpr>(std::get<double>(lit));
    }
    if (expr) {
      expr->line = line;
      return expr;
    }
  }
  if (match({TokenType::FUNCTION})) {
    return lambdaExpression(line);
  }
  if (match({TokenType::STRING})) {
    auto expr = std::make_unique<LiteralExpr>(
        std::get<std::string>(previous().literal));
    expr->line = line;
    return expr;
  }
  if (match({TokenType::TRUE})) {
    auto expr = std::make_unique<LiteralExpr>(true);
    expr->line = line;
    return expr;
  }
  if (match({TokenType::FALSE})) {
    auto expr = std::make_unique<LiteralExpr>(false);
    expr->line = line;
    return expr;
  }
  if (match({TokenType::NIL})) {
    auto expr = std::make_unique<NilExpr>();
    expr->line = line;
    return expr;
  }
  if (match({TokenType::IDENTIFIER})) {
    Token ident = previous();
    
    // check if it's followed by '{'
    if (check(TokenType::LBRACE)) {
      // check if this identifier is a known struct
      if (declaredStructs.count(ident.lexeme) > 0) {
        // this is a struct constructor
        auto constructor = structConstructor();
        if (constructor) {
          return constructor;
        }
      } else {
        auto expr = std::make_unique<VarExpr>(ident.lexeme);
        expr->line = line;

        if (match({TokenType::LBRACE})) {
          error("Unexpected '{' after identifier", peek().line);
          return expr;
        }
        return expr;
      }
    }
    
    // check if it's a function call
    if (check(TokenType::LPAREN)) {
      return functionCall();
    }

    // regular identifier
    auto expr = std::make_unique<VarExpr>(ident.lexeme);
    expr->line = line;
    return expr;
  }
  if (match({TokenType::LPAREN})) {
    auto expr = expression();
    if (!match({TokenType::RPAREN})) {
      error("Expected ')' after expression", line);
    }
    return expr;
  }

  error("Expected expression", line);
  auto expr = std::make_unique<LiteralExpr>((int64_t)0);
  expr->line = line;
  return expr;
}

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

        // check if it's a previously declared struct
        if (declaredStructs.count(typeName) > 0) {
          fieldType = ValueType::STRUCT;
          structTypeName = typeName;
        }
        // otherwise try to match built-in types
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
      // otherwise use the existing parseType() for token-based types
      else {
        fieldType = parseType();
      }

      // check for optional marker
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

  // check if it's empty constructor with defaults: Point {}
  if (match({TokenType::RBRACE})) {
    constructor->useDefaults = true;
    return constructor;
  }

  size_t saved = current;
  skipNewlines();
  bool isNamed = false;
  if (check(TokenType::IDENTIFIER)) {
    size_t lookahead = current + 1;
    while (lookahead < tokens.size() &&
           tokens[lookahead].type == TokenType::NEWLINE) {
      lookahead++;
    }
    if (lookahead < tokens.size() &&
        tokens[lookahead].type == TokenType::ASSIGN) {
      isNamed = true;
    }
  }
  current = saved;

  if (isNamed) {
    // parse named arguments: Point { x = 5, y = 2 }
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
      skipNewlines();

      if (!check(TokenType::IDENTIFIER)) {
        error("Expected field name in struct constructor", peek().line);
        return nullptr;
      }

      Token fieldName = advance();

      if (!match({TokenType::ASSIGN})) {
        error("Expected '=' after field name", peek().line);
        return nullptr;
      }

      auto arg = expression();
      constructor->namedArgs.emplace_back(fieldName.lexeme, std::move(arg));

      skipNewlines();
      if (!check(TokenType::RBRACE) && !match({TokenType::COMMA})) {
        error("Expected ',' or '}' after field assignment", peek().line);
        return nullptr;
      }
    }

    if (!match({TokenType::RBRACE})) {
      error("Expected '}' after struct constructor", peek().line);
      return nullptr;
    }
  } else {
    // parse positional arguments: Point { 5, 2 }
    do {
      auto arg = expression();
      constructor->positionalArgs.push_back(std::move(arg));
      skipNewlines();
    } while (match({TokenType::COMMA}));

    if (!match({TokenType::RBRACE})) {
      error("Expected '}' after struct constructor arguments", peek().line);
      return nullptr;
    }
  }

  return constructor;
}

bool Parser::peekNextIs(TokenType type) {
  if (isAtEnd() || current + 1 >= tokens.size())
    return false;
  return tokens[current + 1].type == type;
}

} // namespace HolyLua