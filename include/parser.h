#pragma once
#include "ast.h"
#include "token.h"
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace HolyLua {
class Parser {
public:
  explicit Parser(const std::vector<Token> &tokens, const std::string &source);
  Program parse();
  void error(const std::string &msg, int line);

private:
  std::vector<Token> tokens;
  std::string source;
  std::vector<std::string> sourceLines;
  size_t current = 0;
  int functionDepth = 0;
  std::unordered_set<std::string>
      declaredStructs;

  void initSourceLines();
  void showErrorContext(int line);
  bool isAtEnd();
  Token peek();
  Token previous();
  Token advance();
  bool check(TokenType type);
  bool match(std::initializer_list<TokenType> types);
  void skipNewlines();
  std::unique_ptr<ASTNode> statement();
  std::unique_ptr<ASTNode> declaration();
  std::unique_ptr<VarDecl> varDeclaration();
  std::unique_ptr<FunctionDecl> functionDeclaration();
  std::unique_ptr<PrintStmt> printStatement();
  std::unique_ptr<IfStmt> ifStatement();
  std::unique_ptr<ReturnStmt> returnStatement();
  std::unique_ptr<FunctionCall> functionCall();
  std::unique_ptr<WhileStmt> whileStatement();
  std::unique_ptr<RepeatStmt> repeatStatement();
  std::unique_ptr<ForStmt> forStatement();
  std::unique_ptr<InlineCStmt> inlineCStatement();
  std::unique_ptr<StructDecl> structDeclaration();
  std::unique_ptr<StructConstructor> structConstructor();
  
  ValueType parseType();

  std::unique_ptr<Expr> expression();
  std::unique_ptr<Expr> nilCoalescing();
  std::unique_ptr<Expr> comparison();
  std::unique_ptr<Expr> concat();
  std::unique_ptr<Expr> additive();
  std::unique_ptr<Expr> multiplicative();
  std::unique_ptr<Expr> unary();
  std::unique_ptr<Expr> postfix();
  std::unique_ptr<Expr> primary();
  std::unique_ptr<Expr> power();
  std::unique_ptr<Expr> logicalOr();
  std::unique_ptr<Expr> logicalAnd();
  std::unique_ptr<LambdaExpr> lambdaExpression(int line);
  std::unique_ptr<FieldAccessExpr> fieldAccess();
  bool peekNextIs(TokenType type);
};
} // namespace HolyLua