#pragma once
#include "ast.h"
#include "common.h"
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace HolyLua {

class Compiler {
public:
  Compiler(const std::string &source);
  std::string compile(const Program &program);

private:
  std::unordered_map<std::string, Variable> symbolTable;
  std::unordered_map<std::string, FunctionInfo> functionTable;
  std::string output;
  std::string source;
  std::vector<std::string> sourceLines;
  int indentLevel = 1;
  std::string currentFunction;
  std::unordered_set<std::string> nonNilVars;
  std::vector<std::unordered_set<std::string>> nonNilVarStack;
  std::string nestedFunctionDecls;
  std::vector<std::pair<std::string, ValueType>> currentFunctionParams;
  std::map<std::string, std::string> structDefs;
  std::map<std::string, StructInfo> structTable;

  void pushScope();
  void popScope();
  void markNonNil(const std::string &varName);
  bool isProvenNonNil(const std::string &varName);

  void initSourceLines();
  void error(const std::string &msg, int line);
  void showErrorContext(int line);
  std::string indent();

  void compileStatement(const ASTNode *node);
  void compileVarDecl(const VarDecl *decl);
  void compileFunctionDecl(const FunctionDecl *func);
  void compileNestedFunction(
      const FunctionDecl *func,
      const std::vector<std::pair<std::string, ValueType>> &parentParams);
  void compileReturnStmt(const ReturnStmt *ret);
  void compileAssignment(const Assignment *assign);
  void compilePrintStmt(const PrintStmt *print);
  void compileIfStmt(const IfStmt *ifStmt);
  void compileWhileStmt(const WhileStmt *whileStmt);
  void compileForStmt(const ForStmt *forStmt);
  void compileRepeatStmt(const RepeatStmt *repeatStmt);
  void compileInlineCStmt(const InlineCStmt *stmt);
  bool containsVariables(const Expr *expr);

  bool collectStructDeclarations(const Program &program);

  void compileStructDecl(const StructDecl *decl);
  std::string compileStructConstructor(const StructConstructor *expr);
  std::string compileStructInitializer(const StructConstructor *expr);
  std::string compileFieldAccess(const FieldAccessExpr *expr);

  std::string compileExpr(const Expr *expr,
                          ValueType expectedType = ValueType::INFERRED,
                          bool forGlobalInit = false);
  std::string compileFunctionCall(const FunctionCall *call);
  std::string generateNilCheck(const std::string &varName, ValueType type);
  ValueType inferExprType(const Expr *expr);
  std::string compileExprForConcat(const Expr *expr);
  void compileLambdaExpr(const LambdaExpr *lambda, std::string &funcName);
  std::string generateUniqueName(const std::string &base);
  std::string getCType(ValueType type);
  std::string getCTypeWithOptional(ValueType type, bool isOptional);
  std::string
  valueToString(const std::variant<int64_t, double, std::string, bool> &value);
  std::string formatString(ValueType type);
  std::string typeToString(ValueType type);
  ValueType
  inferType(const std::variant<int64_t, double, std::string, bool> &value);
  bool checkVariable(const std::string &name);
  bool checkFunction(const std::string &name);
  bool validateExpr(const Expr *expr);
  bool isOptionalExpr(const Expr *expr);
  bool isStringExpr(const Expr *expr);
  bool isNumberExpr(const Expr *expr);
  bool isBoolExpr(const Expr *expr);
  std::string getCTypeForStruct(const std::string &structName);
};

} // namespace HolyLua