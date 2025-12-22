#pragma once
#include "../../ast.h"
#include "../../utils/type_utils.h"
#include "../../utils/error_reporter.h"
#include "variable_collector.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace HolyLua {

class FunctionValidator {
public:
    FunctionValidator(ErrorReporter &reporter);
    
    bool collectFunctionSignature(FunctionDecl *func,
                                 std::unordered_map<std::string, FunctionInfo> &functionTable);
    
    bool inferAndValidateFunction(FunctionDecl *func,
                                 std::unordered_map<std::string, TypeInfo> &symbolTable,
                                 std::unordered_map<std::string, FunctionInfo> &functionTable,
                                 VariableCollector &varCollector);
    
    bool validateFunctionBody(FunctionDecl *func,
                                    std::unordered_map<std::string, TypeInfo> &symbolTable,
                                    std::unordered_map<std::string, FunctionInfo> &functionTable,
                                    const std::map<std::string, StructInfo> &structTable,
                                    const std::map<std::string, ClassInfo> &classTable,
                                    std::string &currentFunction);
    
    ReturnAnalysis analyzeReturnTypes(const std::vector<std::unique_ptr<ASTNode>> &body,
                                     const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                     const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                     const std::map<std::string, StructInfo> &structTable,
                                     const std::map<std::string, ClassInfo> &classTable);
    
private:
    ErrorReporter &reporter;
    
    std::optional<ValueType> inferTypeFromUsage(const std::string &paramName,
                                               const std::vector<std::unique_ptr<ASTNode>> &body,
                                               std::vector<UsageConstraint> &constraints);
    
    void collectUsageConstraints(const std::string &paramName,
                                const ASTNode *node,
                                std::vector<UsageConstraint> &constraints);
    
    void collectExprConstraints(const std::string &paramName,
                               const Expr *expr,
                               std::vector<UsageConstraint> &constraints,
                               ValueType expectedType = ValueType::INFERRED);
    
    bool isParameterInExpr(const std::string &paramName, const Expr *expr);
    bool isParameterInNode(const std::string &paramName, const ASTNode *node);
};

} // namespace HolyLua