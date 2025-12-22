#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace HolyLua {

struct StructDecl;

enum class ValueType { NUMBER, STRING, BOOL, INFERRED, FUNCTION, STRUCT, ENUM };

std::string valueTypeToString(ValueType type);

enum class Visibility {
  PUBLIC,
  PRIVATE
};

enum class BinaryOp {
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  MODULO,
  POWER,
  FLOOR_DIVIDE,
  EQUAL,
  NOT_EQUAL,
  LESS,
  LESS_EQUAL,
  GREATER,
  GREATER_EQUAL,
  CONCAT,
  NIL_COALESCE,
  AND,
  OR
};

enum class UnaryOp { NEGATE, NOT };

struct ASTNode {
  int line = 0;
  virtual ~ASTNode() = default;
};

struct Expr : public ASTNode {
  virtual ~Expr() = default;
};

struct LiteralExpr : public Expr {
  std::variant<int64_t, double, std::string, bool> value;
  LiteralExpr(std::variant<int64_t, double, std::string, bool> v) : value(v) {}
};

struct VarExpr : public Expr {
  std::string name;
  VarExpr(std::string n) : name(n) {}
};

struct FunctionCall : public Expr {
  std::string name;
  std::vector<std::unique_ptr<Expr>> arguments;

  FunctionCall(std::string n, std::vector<std::unique_ptr<Expr>> args)
      : name(n), arguments(std::move(args)) {}
};

struct BinaryExpr : public Expr {
  std::unique_ptr<Expr> left;
  BinaryOp op;
  std::unique_ptr<Expr> right;
  BinaryExpr(std::unique_ptr<Expr> l, BinaryOp o, std::unique_ptr<Expr> r)
      : left(std::move(l)), op(o), right(std::move(r)) {}
};

struct UnaryExpr : public Expr {
  UnaryOp op;
  std::unique_ptr<Expr> operand;
  UnaryExpr(UnaryOp o, std::unique_ptr<Expr> operand)
      : op(o), operand(std::move(operand)) {}
};

struct ForceUnwrapExpr : public Expr {
  std::unique_ptr<Expr> operand;
  ForceUnwrapExpr(std::unique_ptr<Expr> operand)
      : operand(std::move(operand)) {}
};

struct NilExpr : public Expr {
  NilExpr() = default;
};

struct VarDecl : public ASTNode {
  bool isGlobal;
  bool isConst;
  std::string name;
  ValueType type;
  bool isOptional;
  std::unique_ptr<Expr> value;
  bool hasValue;
  std::string typeName;

  VarDecl(bool global, bool cnst, std::string n, ValueType t,
          bool optional = false)
      : isGlobal(global), isConst(cnst), name(n), type(t), isOptional(optional),
        hasValue(false), typeName("") {}
};

struct FunctionDecl : public ASTNode {
  std::string name;
  std::vector<std::pair<std::string, ValueType>> parameters;
  std::vector<bool> parameterOptionals;
  ValueType returnType;
  std::vector<std::unique_ptr<ASTNode>> body;
  bool isGlobal;

  FunctionDecl(std::string n,
               std::vector<std::pair<std::string, ValueType>> params,
               ValueType retType, bool global = false)
      : name(n), parameters(std::move(params)), returnType(retType),
        isGlobal(global) {
    parameterOptionals.resize(parameters.size(), false);
  }
};

struct ReturnStmt : public ASTNode {
  std::unique_ptr<Expr> value;

  ReturnStmt(std::unique_ptr<Expr> v = nullptr) : value(std::move(v)) {}
};

struct Assignment : public ASTNode {
  std::string name;
  std::unique_ptr<Expr> value;
  bool isCompound;
  BinaryOp compoundOp;

  Assignment(std::string n, std::unique_ptr<Expr> v)
      : name(n), value(std::move(v)), isCompound(false) {}

  Assignment(std::string n, std::unique_ptr<Expr> v, BinaryOp op)
      : name(n), value(std::move(v)), isCompound(true), compoundOp(op) {}
};

struct PrintArg {
  bool isIdentifier;
  std::string identifier;
  std::unique_ptr<Expr> expression;

  PrintArg(std::string id) : isIdentifier(true), identifier(id) {}
  PrintArg(std::unique_ptr<Expr> expr)
      : isIdentifier(false), expression(std::move(expr)) {}
};

struct PrintStmt : public ASTNode {
  std::vector<PrintArg> arguments;
  PrintStmt(std::vector<PrintArg> args) : arguments(std::move(args)) {}
};

struct IfStmt : public ASTNode {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<ASTNode>> thenBlock;
  std::vector<std::pair<std::unique_ptr<Expr>, std::vector<std::unique_ptr<ASTNode>>>> elseifBranches;
  std::vector<std::unique_ptr<ASTNode>> elseBlock;

  IfStmt(std::unique_ptr<Expr> cond) : condition(std::move(cond)) {}
};

struct Program {
  std::vector<std::unique_ptr<ASTNode>> statements;
};

struct InlineCStmt : public ASTNode {
  std::string cCode;

  InlineCStmt(std::string code) : cCode(code) {}
};

struct WhileStmt : public ASTNode {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<ASTNode>> body;

  WhileStmt(std::unique_ptr<Expr> cond) : condition(std::move(cond)) {}
};

struct ForStmt : public ASTNode {
  std::string varName;
  std::unique_ptr<Expr> start;
  std::unique_ptr<Expr> end;
  std::unique_ptr<Expr> step;
  std::vector<std::unique_ptr<ASTNode>> body;

  ForStmt(std::string var, std::unique_ptr<Expr> s, std::unique_ptr<Expr> e,
          std::unique_ptr<Expr> st = nullptr)
      : varName(var), start(std::move(s)), end(std::move(e)),
        step(std::move(st)) {}
};

struct RepeatStmt : public ASTNode {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<ASTNode>> body;

  RepeatStmt(std::unique_ptr<Expr> cond) : condition(std::move(cond)) {}
};

struct LambdaExpr : public Expr {
  std::vector<std::pair<std::string, ValueType>> parameters;
  std::vector<bool> parameterOptionals;
  ValueType returnType;
  std::vector<std::unique_ptr<ASTNode>> body;

  LambdaExpr(std::vector<std::pair<std::string, ValueType>> params,
             ValueType retType = ValueType::INFERRED)
      : parameters(std::move(params)), returnType(retType) {
    parameterOptionals.resize(parameters.size(), false);
  }
};

struct StructField {
  std::string name;
  ValueType type;
  bool isOptional;
  bool hasDefault;
  std::variant<int64_t, double, std::string, bool, std::nullptr_t> defaultValue;
  std::string structTypeName;

  StructField(std::string n, ValueType t, bool optional = false,
              bool hasDef = false)
      : name(n), type(t), isOptional(optional), hasDefault(hasDef),
        defaultValue(nullptr), structTypeName("") {}

  StructField(std::string n, ValueType t, std::string structType,
              bool optional = false)
      : name(n), type(t), isOptional(optional), hasDefault(false),
        defaultValue(nullptr), structTypeName(structType) {}

  StructField(std::string n, ValueType t, int64_t defaultVal)
      : name(n), type(t), isOptional(false), hasDefault(true),
        defaultValue(defaultVal), structTypeName("") {}

  StructField(std::string n, ValueType t, double defaultVal)
      : name(n), type(t), isOptional(false), hasDefault(true),
        defaultValue(defaultVal), structTypeName("") {}

  StructField(std::string n, ValueType t, const std::string &defaultVal)
      : name(n), type(t), isOptional(false), hasDefault(true),
        defaultValue(defaultVal), structTypeName("") {}

  StructField(std::string n, ValueType t, bool defaultVal)
      : name(n), type(t), isOptional(false), hasDefault(true),
        defaultValue(defaultVal), structTypeName("") {}
};

struct StructDecl : public ASTNode {
  std::string name;
  std::vector<StructField> fields;

  StructDecl(std::string n) : name(n) {}
};

struct StructConstructor : public Expr {
  std::string structName;
  std::vector<std::pair<std::string, std::unique_ptr<Expr>>> namedArgs;
  std::vector<std::unique_ptr<Expr>> positionalArgs;
  bool useDefaults;

  StructConstructor(std::string name) : structName(name), useDefaults(false) {}
};

struct FieldAccessExpr : public Expr {
  std::unique_ptr<Expr> object;
  std::string fieldName;

  FieldAccessExpr(std::unique_ptr<Expr> obj, std::string field)
      : object(std::move(obj)), fieldName(field) {}
};

struct ClassField {
  Visibility visibility;
  bool isStatic;
  std::string name;
  ValueType type;
  bool isOptional;
  bool hasDefault;
  bool isConst;
  std::variant<int64_t, double, std::string, bool, std::nullptr_t> defaultValue;
  std::string structTypeName;
  
  ClassField(Visibility vis, bool staticMember, std::string n, ValueType t, 
             bool optional = false, bool hasDef = false)
      : visibility(vis), isStatic(staticMember), name(n), type(t), 
        isOptional(optional), hasDefault(hasDef), isConst(false), defaultValue(nullptr), 
        structTypeName("") {}
};

struct ClassMethod {
  Visibility visibility;
  bool isStatic;
  std::string name;
  std::vector<std::pair<std::string, ValueType>> parameters;
  std::vector<bool> parameterOptionals;
  std::vector<std::string> parameterTypeNames;
  ValueType returnType;
  std::vector<std::unique_ptr<ASTNode>> body;
  int line;

  ClassMethod(Visibility vis, bool stat, const std::string &n,
              std::vector<std::pair<std::string, ValueType>> params,
              ValueType ret)
      : visibility(vis), isStatic(stat), name(n), parameters(params),
        returnType(ret), line(0) {}
};

struct ClassDecl : public ASTNode {
  std::string name;
  std::vector<ClassField> fields;
  std::vector<ClassMethod> methods;
  std::unique_ptr<ClassMethod> constructor;
  
  ClassDecl(std::string n) : name(n), constructor(nullptr) {}
  
  ClassDecl(const ClassDecl&) = delete;
  ClassDecl& operator=(const ClassDecl&) = delete;

  ClassDecl(ClassDecl&&) = default;
  ClassDecl& operator=(ClassDecl&&) = default;
};

struct FieldAssignment : public ASTNode {
  std::unique_ptr<Expr> object;
  std::string fieldName;
  std::unique_ptr<Expr> value;
  bool isCompound;
  BinaryOp compoundOp;
  
  FieldAssignment(std::unique_ptr<Expr> obj, std::string field, 
                  std::unique_ptr<Expr> val)
      : object(std::move(obj)), fieldName(field), value(std::move(val)),
        isCompound(false) {}
  
  FieldAssignment(std::unique_ptr<Expr> obj, std::string field,
                  std::unique_ptr<Expr> val, BinaryOp op)
      : object(std::move(obj)), fieldName(field), value(std::move(val)),
        isCompound(true), compoundOp(op) {}
};

struct ClassInstantiation : public Expr {
  std::string className;
  std::vector<std::unique_ptr<Expr>> arguments;
  
  ClassInstantiation(std::string name, std::vector<std::unique_ptr<Expr>> args)
      : className(name), arguments(std::move(args)) {}
};

struct MethodCall : public Expr {
  std::unique_ptr<Expr> object;
  std::string methodName;
  std::vector<std::unique_ptr<Expr>> arguments;
  
  MethodCall(std::unique_ptr<Expr> obj, std::string method,
             std::vector<std::unique_ptr<Expr>> args)
      : object(std::move(obj)), methodName(method), arguments(std::move(args)) {}
};

struct SelfExpr : public Expr {
  SelfExpr() = default;
};

struct EnumDecl : public ASTNode {
  std::string name;
  std::vector<std::string> values;
  
  EnumDecl(std::string n) : name(n) {}
};

struct EnumAccessExpr : public Expr {
  std::string enumName;
  std::string valueName;
  
  EnumAccessExpr(std::string enumN, std::string valN)
      : enumName(enumN), valueName(valN) {}
};


struct ASTPrinter {
  int indentLevel = 0;

  std::string getIndent() const { return std::string(indentLevel * 2, ' '); }

  void print(const ASTNode *node);
  void print(const Program &program);
  void print(const LiteralExpr *expr);
  void print(const VarExpr *expr);
  void print(const SelfExpr *expr);
  void print(const FunctionCall *expr);
  void print(const MethodCall *expr);
  void print(const BinaryExpr *expr);
  void print(const UnaryExpr *expr);
  void print(const ForceUnwrapExpr *expr);
  void print(const LambdaExpr *expr);
  void print(const NilExpr *expr);
  void print(const VarDecl *stmt);
  void print(const FunctionDecl *stmt);
  void print(const ReturnStmt *stmt);
  void print(const Assignment *stmt);
  void print(const FieldAssignment *stmt);
  void print(const PrintStmt *stmt);
  void print(const IfStmt *stmt);
  void print(const InlineCStmt *stmt);
  void print(const WhileStmt *stmt);
  void print(const ForStmt *stmt);
  void print(const RepeatStmt *stmt);
  void print(const StructDecl *stmt);
  void print(const ClassDecl *stmt);
  void print(const StructConstructor *expr);
  void print(const ClassInstantiation *expr);
  void print(const FieldAccessExpr *expr);
  void print(const EnumDecl *stmt);
  void print(const EnumAccessExpr *expr);

  void printBinaryOp(BinaryOp op);
  void printUnaryOp(UnaryOp op);
};

} // namespace HolyLua