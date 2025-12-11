#pragma once
#include "ast.h"
#include "common.h"
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace HolyLua {

class ASTNode;
class Expr;
class FunctionDecl;

struct TypeInfo {
  ValueType type;
  bool isConst;
  bool isDefined;
  bool isOptional;
  bool isFunction;
  bool isStruct;
  std::string structTypeName;
};

struct UsageConstraint {
  ValueType requiredType;
  int line;
  std::string context;

  UsageConstraint(ValueType t, int l, std::string c)
      : requiredType(t), line(l), context(std::move(c)) {}
};

struct ReturnAnalysis {
  std::vector<ValueType> returnTypes;
  std::vector<int> returnLines;
  bool hasConflict;
  ValueType inferredType;

  ReturnAnalysis() : hasConflict(false), inferredType(ValueType::INFERRED) {}
};

class TypeChecker {
public:
  TypeChecker(const std::string &source);
  bool check(const Program &program);
  bool hasErrors() const { return errorCount > 0; }

private:
  std::string source;
  std::vector<std::string> sourceLines;
  std::unordered_map<std::string, TypeInfo> symbolTable;
  std::unordered_map<std::string, FunctionInfo> functionTable;
  std::unordered_set<std::string> nonNilVars;
  std::map<std::string, StructInfo> structTable;
  int errorCount = 0;
  std::string currentFunction;

  void initSourceLines();
  void error(const std::string &msg, int line);
  void showErrorContext(int line);
  void initBuiltinFunctions();

  bool collectStructDeclarations(const Program &program);
  bool collectFunctionSignature(FunctionDecl *func);
  bool inferAndValidateFunction(FunctionDecl *func);
  bool inferParameterTypes(FunctionDecl *func);
  bool collectGlobalDeclarations(const Program &program);

  std::optional<ValueType>
  inferTypeFromUsage(const std::string &paramName,
                     const std::vector<std::unique_ptr<ASTNode>> &body,
                     std::vector<UsageConstraint> &constraints);

  ReturnAnalysis
  analyzeReturnTypes(const std::vector<std::unique_ptr<ASTNode>> &body);

  bool hasAmbiguousUsage(const std::string &paramName,
                         const std::vector<UsageConstraint> &constraints);

  void collectUsageConstraints(const std::string &paramName,
                               const ASTNode *node,
                               std::vector<UsageConstraint> &constraints);

  void collectExprConstraints(const std::string &paramName, const Expr *expr,
                              std::vector<UsageConstraint> &constraints,
                              ValueType expectedType = ValueType::INFERRED);

  bool checkStatement(const ASTNode *node);
  bool checkFunctionDecl(const FunctionDecl *func);
  bool checkNestedFunction(const FunctionDecl *func);
  bool checkReturnStmt(const ReturnStmt *ret);
  bool checkVarDecl(const VarDecl *decl);
  bool checkAssignment(const Assignment *assign);
  bool checkPrintStmt(const PrintStmt *print);
  bool checkIfStmt(const IfStmt *ifStmt);

  ValueType inferExprType(const Expr *expr);
  ValueType getExprType(const Expr *expr);
  bool checkExprType(const Expr *expr, ValueType expected, int line);
  bool isCompatible(ValueType expected, ValueType actual);
  bool canBeNil(const Expr *expr);

  void markNonNil(const std::string &varName);
  bool isProvenNonNil(const std::string &varName);

  std::string typeToString(ValueType type);
  bool operatorRequiresType(BinaryOp op, ValueType &requiredType);
  bool isParameterInExpr(const std::string &paramName, const Expr *expr);
  bool isParameterInNode(const std::string &paramName, const ASTNode *node);
  std::string binaryOpToString(BinaryOp op);

  bool isStructType(const std::string &typeName);
  ValueType resolveTypeName(const std::string &typeName);
};

} // namespace HolyLua