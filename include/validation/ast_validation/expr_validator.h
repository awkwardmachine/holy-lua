#pragma once
#include "../../ast.h"
#include "../../utils/type_utils.h"
#include "../../utils/error_reporter.h"
#include "../../common.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <map>

namespace HolyLua {

class ExpressionValidator {
public:
    ExpressionValidator(ErrorReporter &reporter);
    
    ValueType validateExpression(const Expr *expr,
                                const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                const std::map<std::string, StructInfo> &structTable,
                                const std::map<std::string, ClassInfo> &classTable,
                                const std::string &currentClass = "");
    std::string getFieldStructType(const FieldAccessExpr *field,
                                  const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                  const std::map<std::string, StructInfo> &structTable,
                                  const std::map<std::string, ClassInfo> &classTable,
                                  const std::string &currentClass);
private:
    ErrorReporter &reporter;
    
    ValueType validateLiteral(const LiteralExpr *lit);
    ValueType validateVariable(const VarExpr *var,
                              const std::unordered_map<std::string, TypeInfo> &symbolTable);
    ValueType validateBinaryExpr(const BinaryExpr *bin,
                                const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                const std::map<std::string, StructInfo> &structTable,
                                const std::map<std::string, ClassInfo> &classTable,
                                const std::string &currentClass);
    ValueType validateUnaryExpr(const UnaryExpr *un,
                               const std::unordered_map<std::string, TypeInfo> &symbolTable);
    ValueType validateFunctionCall(const FunctionCall *call,
                                                   const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                   const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                   const std::map<std::string, StructInfo> &structTable,
                                                   const std::map<std::string, ClassInfo> &classTable,
                                                   const std::string &currentClass) ;
    ValueType validateMethodCall(const MethodCall *call,
                                                 const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                 const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                 const std::map<std::string, StructInfo> &structTable,
                                                 const std::map<std::string, ClassInfo> &classTable,
                                                 const std::string &currentClass);
    ValueType validateFieldAccess(const FieldAccessExpr *field,
                                 const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                 const std::map<std::string, StructInfo> &structTable,
                                 const std::map<std::string, ClassInfo> &classTable,
                                 const std::string &currentClass);
    ValueType validateForceUnwrap(const ForceUnwrapExpr *unwrap,
                                 const std::unordered_map<std::string, TypeInfo> &symbolTable);
    ValueType validateClassInstantiation(const ClassInstantiation *inst,
                                        const std::map<std::string, ClassInfo> &classTable);
    ValueType validateStructConstructor(const StructConstructor *cons,
                                                        const std::map<std::string, StructInfo> &structTable,
                                                        const std::map<std::string, ClassInfo> &classTable);
};

} // namespace HolyLua