#include "../../../include/validation/semantics/function_validator.h"
#include "../../../include/validation/semantics/variable_collector.h"
#include "../../../include/validation/ast_validation/expr_validator.h"
#include "../../../include/validation/ast_validation/stmt_validator.h"
#include <unordered_set>

namespace HolyLua {

FunctionValidator::FunctionValidator(ErrorReporter &reporter) 
    : reporter(reporter) {}

bool FunctionValidator::collectFunctionSignature(FunctionDecl *func,
                                               std::unordered_map<std::string, FunctionInfo> &functionTable) {
    if (!func)
        return false;

    // only check for global function conflicts
    if (func->isGlobal && functionTable.count(func->name)) {
        reporter.reportError("Function '" + func->name + "' is already declared", func->line);
        return false;
    }

    // check that all parameters have explicit types
    for (size_t i = 0; i < func->parameters.size(); i++) {
        const auto &param = func->parameters[i];
        if (param.second == ValueType::INFERRED) {
            reporter.reportError("Parameter '" + param.first + "' in function '" + func->name +
                                "' must have an explicit type annotation (e.g., '" +
                                param.first + ": number')", func->line);
            return false;
        }
    }

    FunctionInfo funcInfo;
    funcInfo.name = func->name;
    funcInfo.returnType = func->returnType;
    funcInfo.parameters = func->parameters;
    funcInfo.parameterOptionals = func->parameterOptionals;
    funcInfo.isGlobal = func->isGlobal;
    funcInfo.nestedFunctions = {};

    if (func->isGlobal) {
        functionTable[func->name] = funcInfo;
    }

    return true;
}

bool FunctionValidator::inferAndValidateFunction(FunctionDecl *func,
                                                std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                VariableCollector &varCollector) {
    if (!func)
        return false;

    // only process global functions in this pass
    if (!func->isGlobal) {
        return true;
    }

    // add parameters to symbol table
    auto savedSymbolTable = symbolTable;

    for (size_t i = 0; i < func->parameters.size(); i++) {
        const auto &param = func->parameters[i];
        bool isOptional = func->parameterOptionals[i];

        symbolTable[param.first] = {param.second, false, true, isOptional,
                                    false,        false, ""};
    }

    // collect all local variables before analyzing return types
    varCollector.collectLocalVariables(func->body, symbolTable, 
                                       std::map<std::string, StructInfo>(), 
                                       std::map<std::string, ClassInfo>());

    // analyze return types
    ReturnAnalysis returnAnalysis = analyzeReturnTypes(func->body, symbolTable, 
                                                      functionTable,
                                                      std::map<std::string, StructInfo>(),
                                                      std::map<std::string, ClassInfo>());

    if (returnAnalysis.hasConflict) {
        std::string msg =
            "Function '" + func->name + "' has conflicting return types:\n";
        for (size_t i = 0; i < returnAnalysis.returnTypes.size(); i++) {
            msg += "  Line " + std::to_string(returnAnalysis.returnLines[i]) + ": " +
                   TypeUtils::typeToString(returnAnalysis.returnTypes[i]) + "\n";
        }
        msg += "Explicit return type required or logic must be unified";
        reporter.reportError(msg, func->line);
        symbolTable = savedSymbolTable;
        return false;
    }

    // if return type is inferred, set it
    if (func->returnType == ValueType::INFERRED) {
        if (returnAnalysis.inferredType != ValueType::INFERRED) {
            func->returnType = returnAnalysis.inferredType;
            functionTable[func->name].returnType = returnAnalysis.inferredType;
        } else {
            func->returnType = ValueType::NUMBER;
            functionTable[func->name].returnType = ValueType::NUMBER;
        }
    } else {
        // validate explicit return type matches
        if (returnAnalysis.inferredType != ValueType::INFERRED &&
            !TypeUtils::isCompatible(func->returnType, returnAnalysis.inferredType)) {
            reporter.reportError("Function '" + func->name + "' declared to return " +
                                TypeUtils::typeToString(func->returnType) + " but actually returns " +
                                TypeUtils::typeToString(returnAnalysis.inferredType), func->line);
            symbolTable = savedSymbolTable;
            return false;
        }
    }

    symbolTable = savedSymbolTable;

    return true;
}

bool FunctionValidator::validateFunctionBody(FunctionDecl *func,
                                           std::unordered_map<std::string, TypeInfo> &symbolTable,
                                           std::unordered_map<std::string, FunctionInfo> &functionTable,
                                           const std::map<std::string, StructInfo> &structTable,
                                           const std::map<std::string, ClassInfo> &classTable,
                                           std::string &currentFunction) {
    if (!func)
        return false;

    auto it = functionTable.find(func->name);
    if (it == functionTable.end()) {
        reporter.reportError("Function '" + func->name + "' not found in function table",
                            func->line);
        return false;
    }
    
    auto savedSymbolTable = symbolTable;
    std::string savedFunction = currentFunction;
    currentFunction = func->name;

    // add parameters to symbol table
    for (size_t i = 0; i < func->parameters.size(); i++) {
        const auto &param = func->parameters[i];
        bool isOptional = (i < func->parameterOptionals.size()) ? 
                          func->parameterOptionals[i] : false;
        
        symbolTable[param.first] = {param.second, false, true, isOptional,
                                    false, false, ""};
    }

    for (const auto &stmt : func->body) {
        if (!stmt)
            continue;
            
        if (auto *nestedFunc = dynamic_cast<const FunctionDecl *>(stmt.get())) {
            if (nestedFunc->isGlobal) {
                reporter.reportError("Nested function '" + nestedFunc->name + 
                                   "' cannot be marked as global", nestedFunc->line);
                symbolTable = savedSymbolTable;
                currentFunction = savedFunction;
                return false;
            }
            
            symbolTable[nestedFunc->name] = {ValueType::FUNCTION, false, true, 
                                           false, true, false, ""};
        }
    }

    std::unordered_set<std::string> nonNilVars;
    std::string emptyClass = "";

    StatementValidator stmtValidator(reporter);
    
    // check all statements
    bool hasErrors = false;
    for (const auto &stmt : func->body) {
        if (!stmt)
            continue;

        if (!stmtValidator.validateStatement(stmt.get(), symbolTable, functionTable,
                                           structTable, classTable, nonNilVars,
                                           currentFunction, emptyClass)) {
            hasErrors = true;
        }
    }

    symbolTable = savedSymbolTable;
    currentFunction = savedFunction;

    return !hasErrors;
}

ReturnAnalysis FunctionValidator::analyzeReturnTypes(const std::vector<std::unique_ptr<ASTNode>> &body,
                                                    const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                    const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                    const std::map<std::string, StructInfo> &structTable,
                                                    const std::map<std::string, ClassInfo> &classTable) {
    ReturnAnalysis analysis;

    // create an expression validator for checking return expressions
    ExpressionValidator exprValidator(reporter);
    std::string emptyClass = "";
    
    for (const auto &stmt : body) {
        if (!stmt)
            continue;

        if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
            if (ret->value) {
                ValueType retType = exprValidator.validateExpression(ret->value.get(), symbolTable,
                                                                    functionTable, structTable, 
                                                                    classTable, emptyClass);
                analysis.returnTypes.push_back(retType);
                analysis.returnLines.push_back(ret->line);
            }
        } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(stmt.get())) {
            // recursively check if/else blocks
            ReturnAnalysis thenAnalysis = analyzeReturnTypes(ifStmt->thenBlock, symbolTable,
                                                            functionTable, structTable, classTable);
            ReturnAnalysis elseAnalysis = analyzeReturnTypes(ifStmt->elseBlock, symbolTable,
                                                            functionTable, structTable, classTable);

            analysis.returnTypes.insert(analysis.returnTypes.end(),
                                        thenAnalysis.returnTypes.begin(),
                                        thenAnalysis.returnTypes.end());
            analysis.returnLines.insert(analysis.returnLines.end(),
                                        thenAnalysis.returnLines.begin(),
                                        thenAnalysis.returnLines.end());

            analysis.returnTypes.insert(analysis.returnTypes.end(),
                                        elseAnalysis.returnTypes.begin(),
                                        elseAnalysis.returnTypes.end());
            analysis.returnLines.insert(analysis.returnLines.end(),
                                        elseAnalysis.returnLines.begin(),
                                        elseAnalysis.returnLines.end());
        }
    }

    // check for conflicts
    if (analysis.returnTypes.size() > 1) {
        ValueType firstType = analysis.returnTypes[0];
        for (const auto &type : analysis.returnTypes) {
            if (type != firstType) {
                analysis.hasConflict = true;
                break;
            }
        }
    }

    // set inferred type
    if (!analysis.returnTypes.empty() && !analysis.hasConflict) {
        analysis.inferredType = analysis.returnTypes[0];
    }

    return analysis;
}

std::optional<ValueType> FunctionValidator::inferTypeFromUsage(
    const std::string &paramName,
    const std::vector<std::unique_ptr<ASTNode>> &body,
    std::vector<UsageConstraint> &constraints) {

    // collect all usage constraints
    for (const auto &stmt : body) {
        if (!stmt)
            continue;
        collectUsageConstraints(paramName, stmt.get(), constraints);
    }

    if (constraints.empty()) {
        return std::nullopt;
    }

    // check if all constraints agree on the type
    ValueType inferredType = constraints[0].requiredType;
    for (const auto &constraint : constraints) {
        if (constraint.requiredType != inferredType) {
            return std::nullopt;
        }
    }

    return inferredType;
}

void FunctionValidator::collectUsageConstraints(
    const std::string &paramName, const ASTNode *node,
    std::vector<UsageConstraint> &constraints) {

    if (!node)
        return;

    if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
        if (ret->value) {
            collectExprConstraints(paramName, ret->value.get(), constraints);
        }
    } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
        collectExprConstraints(paramName, assign->value.get(), constraints);
    } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
        for (const auto &arg : print->arguments) {
            if (arg.expression) {
                collectExprConstraints(paramName, arg.expression.get(), constraints);
            }
        }
    } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(node)) {
        collectExprConstraints(paramName, ifStmt->condition.get(), constraints);
        for (const auto &stmt : ifStmt->thenBlock) {
            collectUsageConstraints(paramName, stmt.get(), constraints);
        }
        for (const auto &stmt : ifStmt->elseBlock) {
            collectUsageConstraints(paramName, stmt.get(), constraints);
        }
    } else if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
        if (decl->hasValue && decl->value) {
            collectExprConstraints(paramName, decl->value.get(), constraints);
        }
    } else if (auto *func = dynamic_cast<const FunctionDecl *>(node)) {
        for (const auto &stmt : func->body) {
            collectUsageConstraints(paramName, stmt.get(), constraints);
        }
    }
}

bool FunctionValidator::isParameterInExpr(const std::string &paramName,
                                         const Expr *expr) {
    if (!expr)
        return false;

    if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
        return var->name == paramName;
    } else if (auto *lambda = dynamic_cast<const LambdaExpr *>(expr)) {
        // check if parameter is used inside the lambda body
        for (const auto &stmt : lambda->body) {
            if (isParameterInNode(paramName, stmt.get())) {
                return true;
            }
        }
        return false;
    } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
        return isParameterInExpr(paramName, bin->left.get()) ||
               isParameterInExpr(paramName, bin->right.get());
    } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
        return isParameterInExpr(paramName, un->operand.get());
    } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
        for (const auto &arg : call->arguments) {
            if (isParameterInExpr(paramName, arg.get()))
                return true;
        }
    } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
        return isParameterInExpr(paramName, unwrap->operand.get());
    } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
        for (const auto &arg : structCons->positionalArgs) {
            if (isParameterInExpr(paramName, arg.get()))
                return true;
        }
        for (const auto &namedArg : structCons->namedArgs) {
            if (isParameterInExpr(paramName, namedArg.second.get()))
                return true;
        }
    } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
        return isParameterInExpr(paramName, fieldAccess->object.get());
    }

    return false;
}

bool FunctionValidator::isParameterInNode(const std::string &paramName,
                                         const ASTNode *node) {
    if (!node)
        return false;

    if (auto *expr = dynamic_cast<const Expr *>(node)) {
        return isParameterInExpr(paramName, expr);
    } else if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
        if (ret->value) {
            return isParameterInExpr(paramName, ret->value.get());
        }
    } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
        return isParameterInExpr(paramName, assign->value.get());
    } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
        for (const auto &arg : print->arguments) {
            if (arg.expression &&
                isParameterInExpr(paramName, arg.expression.get())) {
                return true;
            }
        }
    } else if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
        if (decl->hasValue && decl->value) {
            return isParameterInExpr(paramName, decl->value.get());
        }
    }

    return false;
}

void FunctionValidator::collectExprConstraints(
    const std::string &paramName, const Expr *expr,
    std::vector<UsageConstraint> &constraints, ValueType expectedType) {

    if (!expr)
        return;

    if (auto *lambda = dynamic_cast<const LambdaExpr *>(expr)) {
        // check if parameter is used inside the lambda body
        for (const auto &stmt : lambda->body) {
            if (isParameterInNode(paramName, stmt.get())) {
                constraints.emplace_back(ValueType::INFERRED, expr->line,
                                       "used inside lambda expression");
            }
        }
    }

    if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
        if (var->name == paramName) {
            if (expectedType != ValueType::INFERRED) {
                constraints.emplace_back(expectedType, expr->line,
                                       "used in context expecting " +
                                           TypeUtils::typeToString(expectedType));
            }
        }
    } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
        // check if this is a usage of the parameter in a binary operation
        collectExprConstraints(paramName, bin->left.get(), constraints);
        collectExprConstraints(paramName, bin->right.get(), constraints);

        ValueType requiredType;
        if (TypeUtils::operatorRequiresType(bin->op, requiredType)) {
            if (isParameterInExpr(paramName, bin->left.get()) ||
                isParameterInExpr(paramName, bin->right.get())) {
                constraints.emplace_back(
                    requiredType, expr->line,
                    "used with operator '" + TypeUtils::binaryOpToString(bin->op) +
                        "' which requires " + TypeUtils::typeToString(requiredType));
            }
        }
    } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
        if (un->op == UnaryOp::NEGATE) {
            collectExprConstraints(paramName, un->operand.get(), constraints,
                                 ValueType::NUMBER);
        } else {
            collectExprConstraints(paramName, un->operand.get(), constraints);
        }
    } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
        (void)call;
    } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
        collectExprConstraints(paramName, unwrap->operand.get(), constraints);
    } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
        for (const auto &arg : structCons->positionalArgs) {
            collectExprConstraints(paramName, arg.get(), constraints);
        }
        for (const auto &namedArg : structCons->namedArgs) {
            collectExprConstraints(paramName, namedArg.second.get(), constraints);
        }
    } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
        collectExprConstraints(paramName, fieldAccess->object.get(), constraints);
    }
}

} // namespace HolyLua