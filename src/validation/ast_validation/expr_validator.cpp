#include "../../../include/validation/ast_validation/expr_validator.h"

namespace HolyLua {

ExpressionValidator::ExpressionValidator(ErrorReporter &reporter) 
    : reporter(reporter) {}

ValueType ExpressionValidator::validateExpression(const Expr *expr,
                                                const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                const std::map<std::string, StructInfo> &structTable,
                                                const std::map<std::string, ClassInfo> &classTable,
                                                const std::string &currentClass) {
    if (!expr)
        return ValueType::INFERRED;

    if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
        return validateLiteral(lit);
    } else if (dynamic_cast<const NilExpr *>(expr)) {
        return ValueType::INFERRED;
    } else if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
        if (symbolTable.count(var->name)) {
            auto &info = symbolTable.at(var->name);
            if (info.isOptional) {
                return info.type;
            }
        }
        return validateVariable(var, symbolTable);
    } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
        return validateFunctionCall(call, symbolTable, functionTable, structTable, classTable, currentClass);
    } else if (auto *methodCall = dynamic_cast<const MethodCall *>(expr)) {
        return validateMethodCall(methodCall, symbolTable, functionTable, 
                                 structTable, classTable, currentClass);
    } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
        return validateBinaryExpr(bin, symbolTable, functionTable, structTable, classTable, currentClass);
    } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
        return validateUnaryExpr(un, symbolTable);
    } else if (auto *field = dynamic_cast<const FieldAccessExpr *>(expr)) {
        return validateFieldAccess(field, symbolTable, structTable, classTable, currentClass);
    } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
        return validateForceUnwrap(unwrap, symbolTable);
    } else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(expr)) {
        return validateClassInstantiation(classInst, classTable);
    } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
        if (!structTable.count(structCons->structName) && !classTable.count(structCons->structName)) {
            reporter.reportError("Struct/Class '" + structCons->structName + "' is not defined", expr->line);
            return ValueType::INFERRED;
        }
        return ValueType::STRUCT;
    } else if (dynamic_cast<const SelfExpr *>(expr)) {
        return ValueType::STRUCT;
    } else if (dynamic_cast<const LambdaExpr *>(expr)) {
        return ValueType::FUNCTION;
    }

    return ValueType::INFERRED;
}

ValueType ExpressionValidator::validateLiteral(const LiteralExpr *lit) {
    if (std::holds_alternative<int64_t>(lit->value) ||
        std::holds_alternative<double>(lit->value)) {
        return ValueType::NUMBER;
    } else if (std::holds_alternative<std::string>(lit->value)) {
        return ValueType::STRING;
    } else if (std::holds_alternative<bool>(lit->value)) {
        return ValueType::BOOL;
    }
    return ValueType::INFERRED;
}

ValueType ExpressionValidator::validateVariable(const VarExpr *var,
                                               const std::unordered_map<std::string, TypeInfo> &symbolTable) {
    if (symbolTable.count(var->name)) {
        return symbolTable.at(var->name).type;
    }
    reporter.reportError("Variable '" + var->name + "' is not declared", var->line);
    return ValueType::INFERRED;
}

ValueType ExpressionValidator::validateBinaryExpr(const BinaryExpr *bin,
                                                 const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                 const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                 const std::map<std::string, StructInfo> &structTable,
                                                 const std::map<std::string, ClassInfo> &classTable,
                                                 const std::string &currentClass) {
    if (bin->op == BinaryOp::NIL_COALESCE) {
        ValueType leftType = validateExpression(bin->left.get(), symbolTable, functionTable, 
                                               structTable, classTable, currentClass);
        validateExpression(bin->right.get(), symbolTable, functionTable, 
                          structTable, classTable, currentClass);

        bool leftIsOptional = false;
        if (auto *v = dynamic_cast<const VarExpr *>(bin->left.get()))
            if (symbolTable.count(v->name))
                leftIsOptional = symbolTable.at(v->name).isOptional;

        if (!leftIsOptional)
            reporter.reportError("Left side of ?? must be an optional type", bin->line);

        return leftType;
    }

    ValueType leftType = validateExpression(bin->left.get(), symbolTable, functionTable, 
                                           structTable, classTable, currentClass);
    ValueType rightType = validateExpression(bin->right.get(), symbolTable, functionTable, 
                                            structTable, classTable, currentClass);

    if (bin->op == BinaryOp::ADD || bin->op == BinaryOp::SUBTRACT ||
        bin->op == BinaryOp::MULTIPLY || bin->op == BinaryOp::DIVIDE ||
        bin->op == BinaryOp::MODULO) {
        if (leftType != ValueType::NUMBER && leftType != ValueType::INFERRED)
            reporter.reportError("Left operand must be a number", bin->line);
        if (rightType != ValueType::NUMBER && rightType != ValueType::INFERRED)
            reporter.reportError("Right operand must be a number", bin->line);
        return ValueType::NUMBER;
    }

    if (bin->op == BinaryOp::CONCAT) {
        if (leftType != ValueType::STRING && leftType != ValueType::INFERRED)
            reporter.reportError("Left operand must be a string", bin->line);
        if (rightType != ValueType::STRING && rightType != ValueType::INFERRED)
            reporter.reportError("Right operand must be a string", bin->line);
        return ValueType::STRING;
    }

    if (bin->op == BinaryOp::EQUAL || bin->op == BinaryOp::NOT_EQUAL ||
        bin->op == BinaryOp::LESS || bin->op == BinaryOp::LESS_EQUAL ||
        bin->op == BinaryOp::GREATER || bin->op == BinaryOp::GREATER_EQUAL) {
        if (leftType != ValueType::INFERRED &&
            rightType != ValueType::INFERRED &&
            !TypeUtils::isCompatible(leftType, rightType)) {
            reporter.reportError("Cannot compare " + TypeUtils::typeToString(leftType) + 
                               " with " + TypeUtils::typeToString(rightType), bin->line);
        }
        return ValueType::BOOL;
    }

    return ValueType::INFERRED;
}

ValueType ExpressionValidator::validateUnaryExpr(const UnaryExpr *un,
                                                const std::unordered_map<std::string, TypeInfo> &symbolTable) {
    ValueType operandType = validateExpression(un->operand.get(), symbolTable, 
                                              std::unordered_map<std::string, FunctionInfo>(),
                                              std::map<std::string, StructInfo>(),
                                              std::map<std::string, ClassInfo>(), "");
    
    if (un->op == UnaryOp::NEGATE) {
        if (operandType != ValueType::NUMBER && operandType != ValueType::INFERRED)
            reporter.reportError("Cannot negate non-numeric value", un->line);
        return ValueType::NUMBER;
    }
    if (un->op == UnaryOp::NOT)
        return ValueType::BOOL;
    
    return ValueType::INFERRED;
}

ValueType ExpressionValidator::validateFunctionCall(const FunctionCall *call,
                                                   const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                   const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                   const std::map<std::string, StructInfo> &structTable,
                                                   const std::map<std::string, ClassInfo> &classTable,
                                                   const std::string &currentClass) {
    if (symbolTable.count(call->name)) {
        auto &varInfo = symbolTable.at(call->name);
        if (varInfo.isFunction || varInfo.type == ValueType::FUNCTION) {
            for (const auto &arg : call->arguments)
                validateExpression(arg.get(), symbolTable, functionTable, structTable, classTable, currentClass);
            return ValueType::INFERRED;
        }
    }

    if (call->name == "tostring") {
        if (call->arguments.size() != 1)
            reporter.reportError("tostring() expects exactly 1 argument", call->line);
        validateExpression(call->arguments[0].get(), symbolTable, functionTable, structTable, classTable, currentClass);
        return ValueType::STRING;
    }

    if (call->name == "tonumber") {
        if (call->arguments.size() != 1)
            reporter.reportError("tonumber() expects exactly 1 argument", call->line);
        ValueType argType = validateExpression(call->arguments[0].get(), symbolTable, functionTable, structTable, classTable, currentClass);
        if (argType != ValueType::STRING && argType != ValueType::INFERRED)
            reporter.reportError("tonumber() expects a string argument", call->line);
        return ValueType::NUMBER;
    }

    if (call->name == "type") {
        if (call->arguments.size() != 1)
            reporter.reportError("type() expects exactly 1 argument", call->line);
        validateExpression(call->arguments[0].get(), symbolTable, functionTable, structTable, classTable, currentClass);
        return ValueType::STRING;
    }

    if (call->name == "print") {
        for (const auto &arg : call->arguments)
            validateExpression(arg.get(), symbolTable, functionTable, structTable, classTable, currentClass);
        return ValueType::INFERRED;
    }

    if (functionTable.count(call->name)) {
        ValueType ret = functionTable.at(call->name).returnType;
        return ret == ValueType::INFERRED ? ValueType::NUMBER : ret;
    }

    reporter.reportError("Function '" + call->name + "' is not declared", call->line);
    return ValueType::INFERRED;
}

ValueType ExpressionValidator::validateMethodCall(const MethodCall *call,
                                                 const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                 const std::unordered_map<std::string, FunctionInfo> &functionTable,
                                                 const std::map<std::string, StructInfo> &structTable,
                                                 const std::map<std::string, ClassInfo> &classTable,
                                                 const std::string &currentClass) {
    std::string className;
    bool isStaticCall = false;

    // check if this is a static method call
    if (auto *varExpr = dynamic_cast<const VarExpr *>(call->object.get())) {
        if (classTable.count(varExpr->name)) {
            className = varExpr->name;
            isStaticCall = true;
        } else if (symbolTable.count(varExpr->name)) {
            // this is a method call on an instance variable
            const auto &varInfo = symbolTable.at(varExpr->name);
            className = varInfo.structTypeName;
        } else {
            reporter.reportError("Variable/Class '" + varExpr->name + "' is not declared", call->line);
            return ValueType::INFERRED;
        }
    } else if (dynamic_cast<const SelfExpr *>(call->object.get())) {
        // calling on self
        className = currentClass;
    } else {
        reporter.reportError("Cannot determine object type for method call '" +
                            call->methodName + "'", call->line);
        return ValueType::INFERRED;
    }

    if (className.empty()) {
        reporter.reportError("Cannot determine class type for method call '" +
                            call->methodName + "'", call->line);
        return ValueType::INFERRED;
    }
    
    if (!classTable.count(className)) {
        reporter.reportError("Class '" + className + "' is not defined",
            call->line);
        return ValueType::INFERRED;
    }

    auto &classInfo = classTable.at(className);
    
    // check if method exists
    if (!classInfo.methodInfo.count(call->methodName)) {
        reporter.reportError("Method '" + call->methodName +
                            "' does not exist in class '" + className + "'", call->line);
        return ValueType::INFERRED;
    }

    // get method info
    auto methodInfoPair = classInfo.methodInfo.at(call->methodName);
    Visibility methodVisibility = methodInfoPair.second;
    
    // for static calls, check if method is static
    if (isStaticCall) {
        bool methodIsStatic = false;
    }
    
    // check visibility for instance methods
    if (!isStaticCall && call->methodName != "__init") {
        if (methodVisibility == Visibility::PRIVATE && currentClass != className) {
            reporter.reportError("Cannot call private method '" + call->methodName + 
                              "' from outside class '" + className + "'", call->line);
            return ValueType::INFERRED;
        }
    }

    // validate arguments
    for (const auto &arg : call->arguments)
        validateExpression(arg.get(), symbolTable, functionTable, structTable, classTable, currentClass);

    return methodInfoPair.first;
}

ValueType ExpressionValidator::validateFieldAccess(const FieldAccessExpr *field,
                                                  const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                  const std::map<std::string, StructInfo> &structTable,
                                                  const std::map<std::string, ClassInfo> &classTable,
                                                  const std::string &currentClass) {
    ValueType objType = validateExpression(field->object.get(), symbolTable,
                                          std::unordered_map<std::string, FunctionInfo>(),
                                          structTable, classTable, currentClass);

    if (objType != ValueType::STRUCT) {
        reporter.reportError("Cannot access field on non-struct type", field->line);
        return ValueType::INFERRED;
    }

    std::string containerName = "";
    
    if (auto *v = dynamic_cast<const VarExpr *>(field->object.get())) {
        if (symbolTable.count(v->name)) {
            containerName = symbolTable.at(v->name).structTypeName;
        }
    }
    else if (dynamic_cast<const SelfExpr *>(field->object.get())) {
        containerName = currentClass;
    }
    else if (auto *innerField = dynamic_cast<const FieldAccessExpr *>(field->object.get())) {
        containerName = getFieldStructType(innerField, symbolTable, structTable, classTable, currentClass);
    }

    if (containerName.empty()) {
        reporter.reportError("Cannot determine container type for field '" + field->fieldName + "'", 
                           field->line);
        return ValueType::INFERRED;
    }

    if (structTable.count(containerName)) {
        auto &info = structTable.at(containerName);

        for (auto &f : info.fields) {
            if (f.name == field->fieldName) {
                return f.type;
            }
        }

        reporter.reportError("Struct '" + containerName + "' has no field '" + field->fieldName + "'",
                          field->line);
        return ValueType::INFERRED;
    }
    
    if (classTable.count(containerName)) {
        auto &classInfo = classTable.at(containerName);

        // check field visibility
        for (const auto &f : classInfo.fields) {
            if (f.name == field->fieldName) {
                // check if field is private
                if (f.visibility == Visibility::PRIVATE && currentClass != containerName) {
                    reporter.reportError("Cannot access private field '" + field->fieldName + 
                                      "' from outside class '" + containerName + "'", field->line);
                    return ValueType::INFERRED;
                }
                return f.type;
            }
        }
        
        reporter.reportError("Class '" + containerName + "' has no field '" + field->fieldName + "'",
                          field->line);
        return ValueType::INFERRED;
    }
    
    reporter.reportError("Unknown struct/class type '" + containerName + "' for field '" + 
                       field->fieldName + "'", field->line);
    return ValueType::INFERRED;
}

ValueType ExpressionValidator::validateForceUnwrap(const ForceUnwrapExpr *unwrap,
                                                  const std::unordered_map<std::string, TypeInfo> &symbolTable) {
    ValueType operandType = validateExpression(unwrap->operand.get(), symbolTable,
                                              std::unordered_map<std::string, FunctionInfo>(),
                                              std::map<std::string, StructInfo>(),
                                              std::map<std::string, ClassInfo>(), "");
    
    // check if operand is optional
    bool isOptional = false;
    if (auto *var = dynamic_cast<const VarExpr *>(unwrap->operand.get())) {
        if (symbolTable.count(var->name)) {
            isOptional = symbolTable.at(var->name).isOptional;
        }
    }
    
    if (!isOptional) {
        reporter.reportError("Cannot force unwrap (!) non-optional value", unwrap->line);
    }
    
    return operandType;
}

ValueType ExpressionValidator::validateClassInstantiation(const ClassInstantiation *inst,
                                                         const std::map<std::string, ClassInfo> &classTable) {
    if (!classTable.count(inst->className)) {
        reporter.reportError("Class '" + inst->className + "' is not defined", inst->line);
        return ValueType::INFERRED;
    }
    return ValueType::STRUCT;
}

ValueType ExpressionValidator::validateStructConstructor(const StructConstructor *cons,
                                                        const std::map<std::string, StructInfo> &structTable,
                                                        const std::map<std::string, ClassInfo> &classTable) {
    if (!structTable.count(cons->structName) && !classTable.count(cons->structName)) {
        reporter.reportError("Struct/Class '" + cons->structName + "' is not defined", cons->line);
        return ValueType::INFERRED;
    }
    return ValueType::STRUCT;
}

std::string ExpressionValidator::getFieldStructType(const FieldAccessExpr *field,
                                                   const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                   const std::map<std::string, StructInfo> &structTable,
                                                   const std::map<std::string, ClassInfo> &classTable,
                                                   const std::string &currentClass) {
    if (!field)
        return "";
        
    std::string objectStructName;
    
    if (auto *v = dynamic_cast<const VarExpr *>(field->object.get())) {
        if (symbolTable.count(v->name)) {
            objectStructName = symbolTable.at(v->name).structTypeName;
        }
    }
    else if (dynamic_cast<const SelfExpr *>(field->object.get())) {
        objectStructName = currentClass;
    }
    else if (auto *innerField = dynamic_cast<const FieldAccessExpr *>(field->object.get())) {
        // recursively get the struct type of the inner field
        objectStructName = getFieldStructType(innerField, symbolTable, structTable, classTable, currentClass);
    }
    
    if (objectStructName.empty()) {
        return "";
    }
    
    if (structTable.count(objectStructName)) {
        auto &info = structTable.at(objectStructName);
        for (const auto &f : info.fields) {
            if (f.name == field->fieldName) {
                if (f.type == ValueType::STRUCT && !f.structTypeName.empty()) {
                    return f.structTypeName;
                }
                return objectStructName;
            }
        }
    }
    else if (classTable.count(objectStructName)) {
        auto &classInfo = classTable.at(objectStructName);
        for (const auto &f : classInfo.fields) {
            if (f.name == field->fieldName) {
                if (f.type == ValueType::STRUCT && !f.structTypeName.empty()) {
                    return f.structTypeName;
                }
                return objectStructName;
            }
        }
    }
    
    return "";
}

} // namespace HolyLua