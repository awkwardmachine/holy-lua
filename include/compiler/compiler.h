#pragma once
#include "../ast.h"
#include "../common.h"
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
  std::vector<std::pair<std::string, std::string>> structDefs;
  std::map<std::string, StructInfo> structTable;
  std::map<std::string, ClassInfo> classTable;
  std::string currentClass;
  std::map<std::string, std::vector<std::string>> enumTable;

  void initSourceLines();
  std::string indent();

  void error(const std::string &msg, int line);
  void showErrorContext(int line);

  void pushScope();
  void popScope();
  void markNonNil(const std::string &varName);
  bool isProvenNonNil(const std::string &varName);
  bool checkVariable(const std::string &name);
  bool checkFunction(const std::string &name);
  bool validateExpr(const Expr *expr);
  bool isOptionalExpr(const Expr *expr);
  std::string generateUniqueName(const std::string &base);

  std::string getCTypeForStruct(const std::string &structName);
  std::string getCType(ValueType type, const std::string &structTypeName = "");
  std::string getCTypeWithOptional(ValueType type, bool isOptional);
  std::string getCTypeForVar(const std::string &varName);
  std::string typeToString(ValueType type);
  std::string formatString(ValueType type);
  ValueType inferType(const std::variant<int64_t, double, std::string, bool> &value);
  std::string doubleToString(double value);
  std::string valueToString(const std::variant<int64_t, double, std::string, bool> &value);
  bool isStringExpr(const Expr *expr);
  bool isNumberExpr(const Expr *expr);
  bool isBoolExpr(const Expr *expr);

  std::string generateNilCheck(const std::string &varName, ValueType type);

  void compileStatement(const ASTNode *node);
  void compileInlineCStmt(const InlineCStmt *stmt);

  void compileIfStmt(const IfStmt *ifStmt);
  void compileWhileStmt(const WhileStmt *whileStmt);
  void compileForStmt(const ForStmt *forStmt);
  void compileRepeatStmt(const RepeatStmt *repeatStmt);

  void compilePrintStmt(const PrintStmt *print);
  bool validateExprForPrint(const Expr *expr);

  void compileVarDecl(const VarDecl *decl);
  void compileGlobalVarDecl(const VarDecl *decl, std::string &globalDecls,
                            std::vector<std::pair<std::string, std::string>> &deferredInitializations);
  void compileAssignment(const Assignment *assign);
  void compileReturnStmt(const ReturnStmt *ret);

  void compileFunctionDecl(const FunctionDecl *func);
  void compileNestedFunction(const FunctionDecl *func,
                             const std::vector<std::pair<std::string, ValueType>> &parentParams);
  std::string compileFunctionCall(const FunctionCall *call);
  void compileLambdaExpr(const LambdaExpr *lambda, std::string &funcName);

  void compileStructDecl(const StructDecl *decl);
  std::string compileStructConstructor(const StructConstructor *expr);
  std::string compileStructInitializer(const StructConstructor *expr);

  void compileClassDecl(const ClassDecl *decl);
  std::string compileClassInstantiation(const ClassInstantiation *expr);
  std::string compileSelfExpr(const SelfExpr *expr);
  
  void compileConstructor(const std::string &className, const ClassMethod &constructor);
  void compileMethod(const std::string &className, const ClassMethod &method);
  std::string compileMethodCall(const MethodCall *call);
  
  std::string compileFieldAccess(const FieldAccessExpr *expr);
  void compileFieldAssignment(const FieldAssignment *assign);

  std::string compileExpr(const Expr *expr,
                          ValueType expectedType = ValueType::INFERRED,
                          bool forGlobalInit = false);
  std::string compileExprForConcat(const Expr *expr);
  bool containsVariables(const Expr *expr);

  void compileEnumDecl(const EnumDecl *decl);
  std::string compileEnumAccess(const EnumAccessExpr *expr);
  
  ValueType inferExprType(const Expr *expr);
  std::string getStructTypeNameFromFieldAccess(const FieldAccessExpr *fieldAccess);
  ValueType inferFieldAccessType(const FieldAccessExpr *fieldAccess);
};

} // namespace HolyLua