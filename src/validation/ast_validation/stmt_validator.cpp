#include "../../../include/validation/ast_validation/stmt_validator.h"
#include "../../../include/validation/ast_validation/type_compat.h"

namespace HolyLua {

StatementValidator::StatementValidator(ErrorReporter &reporter) 
    : reporter(reporter), exprValidator(reporter) {}

bool StatementValidator::validateStatement(const ASTNode *node,
                                         std::unordered_map<std::string, TypeInfo> &symbolTable,
                                         const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                         const std::map<std::string, StructInfo> &structTable,
                                         const std::map<std::string, ClassInfo> &classTable,
                                         std::unordered_set<std::string> &nonNilVars,
                                         std::string &currentFunction,
                                         std::string &currentClass) {
    if (!node)
        return true;

    
    if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
        return validateVarDecl(decl, symbolTable, structTable, classTable);
    } else if (auto *func = dynamic_cast<const FunctionDecl *>(node)) {
        return true;
    } else if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
        return validateReturnStmt(ret, symbolTable, functionTable, structTable, classTable, currentClass);
    } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
        return validateAssignment(assign, symbolTable, nonNilVars);
    } else if (auto *fieldAssign = dynamic_cast<const FieldAssignment *>(node)) {
        return validateFieldAssignment(fieldAssign, symbolTable, structTable, classTable, currentClass);
    } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
        return validatePrintStmt(print, symbolTable, nonNilVars,
                                structTable, classTable, currentClass);
    } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(node)) {
        return validateIfStmt(ifStmt, symbolTable, functionTable, structTable, classTable, 
                             nonNilVars, currentFunction, currentClass);
    } else if (auto *classDecl = dynamic_cast<const ClassDecl *>(node)) {
        return true;
    } else if (auto *structDecl = dynamic_cast<const StructDecl *>(node)) {
        return true;
    } else if (auto *inlineC = dynamic_cast<const InlineCStmt *>(node)) {
        return true;
    } else if (auto *expr = dynamic_cast<const Expr *>(node)) {
        exprValidator.validateExpression(expr, symbolTable, functionTable, structTable, classTable, currentClass);
        return true;
    }
    
    return true;
}

bool StatementValidator::validateVarDecl(const VarDecl *decl,
                                        std::unordered_map<std::string, TypeInfo> &symbolTable,
                                        const std::map<std::string, StructInfo> &structTable,
                                        const std::map<std::string, ClassInfo> &classTable) {
    if (!decl)
        return false;

    if (decl->hasValue && decl->value) {
        // check if assigning a lambda
        if (auto *lambda = dynamic_cast<const LambdaExpr *>(decl->value.get())) {
            auto savedSymbolTable = symbolTable;
            std::string savedFunction = "";
            std::unordered_set<std::string> emptyNonNilVars;
            std::string emptyClass = "";

            // add lambda parameters to symbol table
            for (size_t i = 0; i < lambda->parameters.size(); i++) {
                const auto &param = lambda->parameters[i];
                bool paramIsOptional = lambda->parameterOptionals[i];
                if (param.second == ValueType::INFERRED) {
                    reporter.reportError("Lambda parameter '" + param.first +
                                        "' must have explicit type", decl->line);
                    return false;
                }
                symbolTable[param.first] = {param.second, false, true, paramIsOptional,
                                            false,        false, ""};
            }

            // check lambda body
            bool hasErrors = false;
            for (const auto &stmt : lambda->body) {
                if (!validateStatement(stmt.get(), symbolTable, 
                                      std::unordered_map<std::string, FunctionInfo>(),
                                      structTable, classTable,
                                      emptyNonNilVars,
                                      savedFunction, emptyClass)) {
                    hasErrors = true;
                }
            }

            // restore context
            symbolTable = savedSymbolTable;

            if (hasErrors) {
                return false;
            }
        }
    }

    return true;
}

bool StatementValidator::validateAssignment(const Assignment *assign,
                                          std::unordered_map<std::string, TypeInfo> &symbolTable,
                                          std::unordered_set<std::string> &nonNilVars) {
    if (!assign)
        return false;

    if (!symbolTable.count(assign->name)) {
        reporter.reportError("Variable '" + assign->name + "' is not declared", assign->line);
        return false;
    }

    auto &varInfo = symbolTable[assign->name];

    if (varInfo.isConst) {
        reporter.reportError("Cannot assign to const variable '" + assign->name + "'",
                           assign->line);
        return false;
    }

    // create empty tables for expression validation
    std::unordered_map<std::string, FunctionInfo> emptyFunctionTable;
    std::map<std::string, StructInfo> emptyStructTable;
    std::map<std::string, ClassInfo> emptyClassTable;
    std::string emptyClass = "";
    
    ValueType valueType = exprValidator.validateExpression(assign->value.get(), symbolTable,
                                                          emptyFunctionTable,
                                                          emptyStructTable,
                                                          emptyClassTable, emptyClass);
    
    bool valueCanBeNil = false;
    if (dynamic_cast<const NilExpr *>(assign->value.get())) {
        valueCanBeNil = true;
    } else if (auto *var = dynamic_cast<const VarExpr *>(assign->value.get())) {
        if (symbolTable.count(var->name)) {
            valueCanBeNil = symbolTable[var->name].isOptional;
        }
    }

    // check if assigning a lambda
    if (auto *lambda = dynamic_cast<const LambdaExpr *>(assign->value.get())) {
        auto savedSymbolTable = symbolTable;
        std::string savedFunction = "";

        // add lambda parameters to symbol table
        for (size_t i = 0; i < lambda->parameters.size(); i++) {
            const auto &param = lambda->parameters[i];
            bool paramIsOptional = lambda->parameterOptionals[i];
            if (param.second == ValueType::INFERRED) {
                reporter.reportError("Lambda parameter '" + param.first + "' must have explicit type",
                                   assign->line);
                return false;
            }
            symbolTable[param.first] = {param.second, false, true, paramIsOptional,
                                        false,        false, ""};
        }

        // check lambda body
        bool hasErrors = false;
        for (const auto &stmt : lambda->body) {
            if (!validateStatement(stmt.get(), symbolTable, 
                                  emptyFunctionTable,
                                  emptyStructTable,
                                  emptyClassTable,
                                  nonNilVars, savedFunction, emptyClass)) {
                hasErrors = true;
            }
        }

        // restore context
        symbolTable = savedSymbolTable;

        if (hasErrors) {
            return false;
        }

        // update variable info to mark as function
        varInfo.type = ValueType::FUNCTION;
        varInfo.isFunction = true;
        valueType = ValueType::FUNCTION;
        valueCanBeNil = false;
    }

    if (assign->isCompound) {
        if (varInfo.isOptional && nonNilVars.count(assign->name) == 0) {
            reporter.reportError("Cannot use compound assignment on optional variable '" +
                                assign->name +
                                "' that might be nil. Use force unwrap (!) or check for nil "
                                "first", assign->line);
            return false;
        }

        if (varInfo.type != ValueType::NUMBER || valueType != ValueType::NUMBER) {
            reporter.reportError("Compound assignment requires number types", assign->line);
            return false;
        }
    } else {
        if (!TypeCompatibility::checkAssignment(assign->name, varInfo, valueType, 
                                               valueCanBeNil, assign->line, reporter)) {
            return false;
        }
    }

    return true;
}

bool StatementValidator::validateFieldAssignment(const FieldAssignment *assign,
                                               std::unordered_map<std::string, TypeInfo> &symbolTable,
                                               const std::map<std::string, StructInfo> &structTable,
                                               const std::map<std::string, ClassInfo> &classTable,
                                               const std::string &currentClass) {
    if (!assign)
        return false;
    
    // create empty function table for expression validation
    std::unordered_map<std::string, FunctionInfo> emptyFunctionTable;
    
    ValueType objectType = exprValidator.validateExpression(assign->object.get(), symbolTable,
                                                           emptyFunctionTable,
                                                           structTable, classTable, currentClass);

    if (objectType != ValueType::STRUCT) {
        reporter.reportError("Cannot access field on non-struct/class type", assign->line);
        return false;
    }
    
    std::string typeName = "";
    
    if (auto *varExpr = dynamic_cast<const VarExpr *>(assign->object.get())) {
        if (symbolTable.count(varExpr->name)) {
            typeName = symbolTable[varExpr->name].structTypeName;
        }
    } else if (dynamic_cast<const SelfExpr *>(assign->object.get())) {
        typeName = currentClass;
    }
    else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(assign->object.get())) {
        typeName = exprValidator.getFieldStructType(fieldAccess, symbolTable, structTable, classTable, currentClass);
    }
    
    if (typeName.empty()) {
        if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(assign->object.get())) {
            // validate the field access to get its type
            ValueType fieldType = exprValidator.validateExpression(fieldAccess, symbolTable,
                                                                 emptyFunctionTable,
                                                                 structTable, classTable, currentClass);

            if (fieldType == ValueType::STRUCT) {
                if (auto *innerField = dynamic_cast<const FieldAccessExpr *>(fieldAccess->object.get())) {
                    std::string innerTypeName = exprValidator.getFieldStructType(innerField, symbolTable, 
                                                                                structTable, classTable, currentClass);

                    if (!innerTypeName.empty() && structTable.count(innerTypeName)) {
                        auto &structInfo = structTable.at(innerTypeName);
                        for (const auto &field : structInfo.fields) {
                            if (field.name == fieldAccess->fieldName) {
                                if (field.type == ValueType::STRUCT && !field.structTypeName.empty()) {
                                    typeName = field.structTypeName;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        if (typeName.empty()) {
            reporter.reportError("Cannot determine type for field assignment", assign->line);
            return false;
        }
    }
    
    if (classTable.count(typeName)) {
        const auto &classInfo = classTable.at(typeName);
        
        if (classInfo.fieldInfo.find(assign->fieldName) == classInfo.fieldInfo.end()) {
            reporter.reportError("Class '" + typeName + "' has no field named '" + 
                               assign->fieldName + "'", assign->line);
            return false;
        }
        
        auto fieldInfoPair = classInfo.fieldInfo.at(assign->fieldName);
        ValueType fieldType = fieldInfoPair.first;
        Visibility fieldVisibility = fieldInfoPair.second;
        
        if (fieldVisibility == Visibility::PRIVATE && currentClass != typeName) {
            reporter.reportError("Cannot access private field '" + assign->fieldName + 
                               "' from outside class '" + typeName + "'", assign->line);
            return false;
        }
        
        ValueType valueType = exprValidator.validateExpression(assign->value.get(), symbolTable,
                                                              emptyFunctionTable,
                                                              structTable, classTable, currentClass);
        if (!TypeUtils::isCompatible(fieldType, valueType)) {
            reporter.reportError("Type mismatch: cannot assign " + TypeUtils::typeToString(valueType) +
                                " to field of type " + TypeUtils::typeToString(fieldType), assign->line);
            return false;
        }
    } else if (structTable.count(typeName)) {
        const auto &structInfo = structTable.at(typeName);
        
        auto fieldIt = structInfo.fieldTypes.find(assign->fieldName);
        if (fieldIt == structInfo.fieldTypes.end()) {
            reporter.reportError("Struct '" + typeName + "' has no field named '" + 
                               assign->fieldName + "'", assign->line);
            return false;
        }
        
        ValueType fieldType = fieldIt->second.first;
        ValueType valueType = exprValidator.validateExpression(assign->value.get(), symbolTable,
                                                              emptyFunctionTable,
                                                              structTable, classTable, currentClass);
        
        if (!TypeUtils::isCompatible(fieldType, valueType)) {
            reporter.reportError("Type mismatch: cannot assign " + TypeUtils::typeToString(valueType) +
                                " to field of type " + TypeUtils::typeToString(fieldType), assign->line);
            return false;
        }
    } else {
        reporter.reportError("Unknown struct/class type '" + typeName + "'", assign->line);
        return false;
    }
    
    return true;
}

bool StatementValidator::validatePrintStmt(const PrintStmt *print,
                                         const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                         const std::unordered_set<std::string> &nonNilVars,
                                         const std::map<std::string, StructInfo> &structTable,
                                         const std::map<std::string, ClassInfo> &classTable,
                                         const std::string &currentClass) {
    if (!print)
        return false;

    for (const auto &arg : print->arguments) {
        if (arg.isIdentifier) {
            if (!symbolTable.count(arg.identifier)) {
                reporter.reportError("Variable '" + arg.identifier + "' is not declared", print->line);
                return false;
            }
            auto &varInfo = symbolTable.at(arg.identifier);
            if (varInfo.isOptional && nonNilVars.count(arg.identifier) == 0) {
                reporter.reportError("Cannot print optional variable '" + arg.identifier +
                                    "' that might be nil. Use force unwrap (!) or check for nil "
                                    "first", print->line);
                return false;
            }
        } else if (arg.expression) {
            std::unordered_map<std::string, FunctionInfo> emptyFunctionTable;
            exprValidator.validateExpression(arg.expression.get(), symbolTable,
                                           emptyFunctionTable,
                                           structTable, classTable, currentClass);
        }
    }
    return true;
}

bool StatementValidator::validateIfStmt(const IfStmt *ifStmt,
                                       std::unordered_map<std::string, TypeInfo> &symbolTable,
                                       const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                       const std::map<std::string, StructInfo> &structTable,
                                       const std::map<std::string, ClassInfo> &classTable,
                                       std::unordered_set<std::string> &nonNilVars,
                                       std::string &currentFunction,
                                       std::string &currentClass) {
    if (!ifStmt)
        return false;

    // validate the condition
    ValueType condType = exprValidator.validateExpression(ifStmt->condition.get(), 
                                                         symbolTable, functionTable,
                                                         structTable, classTable, currentClass);
    
    std::unordered_set<std::string> savedNonNilVars = nonNilVars;

    // check if condition is a simple variable 
    if (auto *varExpr = dynamic_cast<const VarExpr *>(ifStmt->condition.get())) {
        // this is the syntax sugar for checking non-nil
        if (symbolTable.count(varExpr->name)) {
            auto &varInfo = symbolTable.at(varExpr->name);
            if (varInfo.isOptional) {
                nonNilVars.insert(varExpr->name);
            }
        }
    } else if (auto *binExpr = dynamic_cast<const BinaryExpr *>(ifStmt->condition.get())) {
        // handle explicit comparisons like x != nil
        if (binExpr->op == BinaryOp::NOT_EQUAL) {
            if (auto *varExpr = dynamic_cast<const VarExpr *>(binExpr->left.get())) {
                if (dynamic_cast<const NilExpr *>(binExpr->right.get())) {
                    nonNilVars.insert(varExpr->name);
                }
            }
        } else if (binExpr->op == BinaryOp::EQUAL) {
            if (auto *varExpr = dynamic_cast<const VarExpr *>(binExpr->left.get())) {
                if (dynamic_cast<const NilExpr *>(binExpr->right.get())) {
                    
                }
            }
        }
    }

    // validate then block with narrowed types
    for (const auto &stmt : ifStmt->thenBlock) {
        if (!stmt)
            continue;
        if (!validateStatement(stmt.get(), symbolTable, functionTable, structTable, classTable,
                              nonNilVars, currentFunction, currentClass)) {
            return false;
        }
    }

    nonNilVars = savedNonNilVars;

    for (const auto &stmt : ifStmt->elseBlock) {
        if (!stmt)
            continue;
        if (!validateStatement(stmt.get(), symbolTable, functionTable, structTable, classTable,
                              nonNilVars, currentFunction, currentClass)) {
            return false;
        }
    }

    nonNilVars = savedNonNilVars;

    return true;
}

bool StatementValidator::validateReturnStmt(const ReturnStmt *ret,
                                          const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                          const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                          const std::map<std::string, StructInfo> &structTable,
                                          const std::map<std::string, ClassInfo> &classTable,
                                          const std::string &currentClass) {
    if (!ret)
        return true;

    if (ret->value) {
        exprValidator.validateExpression(ret->value.get(), symbolTable, functionTable,
                                        structTable, classTable, currentClass);
    }
    return true;
}

} // namespace HolyLua