#include "../../../include/validation/semantics/class_validator.h"
#include "../../../include/validation/semantics/function_validator.h"
#include "../../../include/validation/semantics/variable_collector.h"
#include "../../../include/validation/ast_validation/stmt_validator.h"

namespace HolyLua {

ClassValidator::ClassValidator(ErrorReporter &reporter) 
    : reporter(reporter) {}

bool ClassValidator::collectClassDeclarations(const Program &program,
                                            std::map<std::string, ClassInfo> &classTable) {
    for (auto &stmt : program.statements) {
        if (!stmt)
            continue;

        if (auto *classDecl = dynamic_cast<ClassDecl *>(stmt.get())) {
            // check if class is already defined
            if (classTable.count(classDecl->name)) {
                reporter.reportError("Class '" + classDecl->name + "' is already defined",
                                   classDecl->line);
                return false;
            }

            // create the ClassInfo object directly in the table using emplace
            auto &info = classTable[classDecl->name];
            info.name = classDecl->name;
            info.fields = classDecl->fields;
            
            for (const auto &field : classDecl->fields) {
                info.fieldInfo[field.name] = {field.type, field.visibility};
            }
            
            for (const auto &method : classDecl->methods) {
                info.methodInfo[method.name] = {method.returnType, method.visibility};
            }
            
            if (classDecl->constructor) {
                info.methodInfo["__init"] = {ValueType::INFERRED, Visibility::PUBLIC};
            }
        }
    }

    return true;
}

bool ClassValidator::validateClassDeclaration(const ClassDecl *decl,
                                            std::map<std::string, ClassInfo> &classTable,
                                            const std::map<std::string, StructInfo> &structTable) {
    if (!decl)
        return false;

    // check for duplicate field names
    std::set<std::string> fieldNames;
    for (const auto &field : decl->fields) {
        if (fieldNames.count(field.name)) {
            reporter.reportError("Duplicate field name '" + field.name + "' in class '" + 
                                decl->name + "'", decl->line);
            return false;
        }
        fieldNames.insert(field.name);
        
        // validate field types
        if (field.type == ValueType::INFERRED) {
            reporter.reportError("Class field '" + field.name + "' must have explicit type", 
                               decl->line);
            return false;
        }
        
        // check if struct/class type is valid 
        if (field.type == ValueType::STRUCT) {
            if (!field.structTypeName.empty()) {
                if (!structTable.count(field.structTypeName) && 
                    !classTable.count(field.structTypeName)) {
                    reporter.reportError("Unknown type '" + field.structTypeName + "' for field '" + 
                                      field.name + "'", decl->line);
                    return false;
                }
            } else {
                reporter.reportError("Struct field '" + field.name + "' missing type name", decl->line);
                return false;
            }
        }
        
        // validate default values for static fields
        if (field.isStatic && field.hasDefault) {
            ValueType defaultType = ValueType::INFERRED;
            
            std::visit(
                [&](auto &&arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, int64_t> || 
                                  std::is_same_v<T, double>) {
                        defaultType = ValueType::NUMBER;
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        defaultType = ValueType::STRING;
                    } else if constexpr (std::is_same_v<T, bool>) {
                        defaultType = ValueType::BOOL;
                    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                        defaultType = ValueType::INFERRED;
                    }
                },
                field.defaultValue);
            
            if (defaultType != ValueType::INFERRED && 
                !TypeUtils::isCompatible(field.type, defaultType)) {
                reporter.reportError("Default value type mismatch for field '" + field.name + 
                                  "': expected " + TypeUtils::typeToString(field.type) + ", got " + 
                                  TypeUtils::typeToString(defaultType), decl->line);
                return false;
            }
        }
    }
    
    // check for duplicate method names
    std::set<std::string> methodNames;
    for (const auto &method : decl->methods) {
        if (methodNames.count(method.name)) {
            reporter.reportError("Duplicate method name '" + method.name + "' in class '" + 
                               decl->name + "'", method.line);
            return false;
        }
        methodNames.insert(method.name);
        
        if (method.name == "__init") {
            reporter.reportError("Method cannot be named '__init' - this is reserved for constructors", 
                               method.line);
            return false;
        }
    }
    
    // check constructor if present
    if (decl->constructor) {
        if (!validateConstructor(decl, decl->constructor.get())) {
            return false;
        }
    }
    
    // check that all public non-static fields are initialized
    if (!checkClassFieldsInitialized(decl)) {
        return false;
    }

    return true;
}

bool ClassValidator::validateClassMethod(const std::string &className,
                                        const ClassMethod &method,
                                        bool isConstructor,
                                        std::unordered_map<std::string, TypeInfo> &symbolTable,
                                        const std::map<std::string, StructInfo> &structTable,
                                        std::map<std::string, ClassInfo> &classTable,
                                        std::unordered_set<std::string> &nonNilVars,
                                        std::string &currentClass,
                                        std::string &currentFunction) {
    
    auto savedSymbolTable = symbolTable;
    auto savedNonNilVars = nonNilVars;
    std::string savedFunction = currentFunction;
    currentFunction = className + "_" + method.name;
    std::string savedCurrentClass = currentClass;
    currentClass = className;
    
    // add self parameter for instance methods
    if (!method.isStatic) {
        symbolTable["self"] = {ValueType::STRUCT, false, true, false, false, true, className};
    }
    
    // check parameters for duplicates and validate types
    for (size_t i = 0; i < method.parameters.size(); i++) {
        const auto &param = method.parameters[i];
        bool isOptional = (i < method.parameterOptionals.size()) ? 
                          method.parameterOptionals[i] : false;
        
        // validate parameter has explicit type
        if (param.second == ValueType::INFERRED) {
            reporter.reportError("Method parameter '" + param.first + "' must have explicit type", method.line);
            symbolTable = savedSymbolTable;
            currentFunction = savedFunction;
            currentClass = savedCurrentClass;
            return false;
        }
        
        // check for duplicate parameter names
        for (size_t j = 0; j < i; j++) {
            if (method.parameters[j].first == param.first) {
                reporter.reportError("Duplicate parameter name '" + param.first + "' in method '" + method.name + "'", method.line);
                symbolTable = savedSymbolTable;
                currentFunction = savedFunction;
                currentClass = savedCurrentClass;
                return false;
            }
        }
        
        std::string structTypeName = "";
        bool isStruct = false;
        
        // handle struct/class type parameters
        if (param.second == ValueType::STRUCT) {
            isStruct = true;
            if (i < method.parameterTypeNames.size() && !method.parameterTypeNames[i].empty()) {
                structTypeName = method.parameterTypeNames[i];
            } else {
                reporter.reportError("Struct parameter '" + param.first + "' in method '" + method.name + 
                                  "' missing type information.", method.line);
                symbolTable = savedSymbolTable;
                currentFunction = savedFunction;
                currentClass = savedCurrentClass;
                return false;
            }
            
            if (!structTable.count(structTypeName) && !classTable.count(structTypeName)) {
                reporter.reportError("Unknown type '" + structTypeName + "' for parameter '" + 
                                  param.first + "'", method.line);
                symbolTable = savedSymbolTable;
                currentFunction = savedFunction;
                currentClass = savedCurrentClass;
                return false;
            }
        }
        
        // add parameter to symbol table
        symbolTable[param.first] = {param.second, false, true, isOptional, 
                                  false, isStruct, structTypeName};
    }
    
    // handle constructor-specific validation
    if (isConstructor) {
        if (method.returnType != ValueType::INFERRED) {
            reporter.reportError("Constructor cannot have explicit return type", method.line);
            symbolTable = savedSymbolTable;
            currentFunction = savedFunction;
            currentClass = savedCurrentClass;
            return false;
        }
        
        for (const auto &stmt : method.body) {
            if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
                if (ret->value) {
                    reporter.reportError("Constructor cannot return a value", ret->line);
                    symbolTable = savedSymbolTable;
                    currentFunction = savedFunction;
                    currentClass = savedCurrentClass;
                    return false;
                }
            }
        }
    } else if (method.returnType == ValueType::INFERRED) {
        VariableCollector varCollector(reporter);
        varCollector.collectLocalVariables(method.body, symbolTable,
                                          structTable, classTable);

        FunctionValidator funcValidator(reporter);
        auto analysis = funcValidator.analyzeReturnTypes(method.body, symbolTable,
                                                        std::unordered_map<std::string, FunctionInfo>(),
                                                        structTable, classTable);
        
        if (analysis.hasConflict) {
            reporter.reportError("Method '" + method.name + "' has conflicting return types", method.line);
            symbolTable = savedSymbolTable;
            currentFunction = savedFunction;
            currentClass = savedCurrentClass;
            return false;
        }
    }
    
    // collect all local variables before checking statements
    VariableCollector varCollector(reporter);
    varCollector.collectLocalVariables(method.body, symbolTable,
                                      structTable, classTable);
    
    // check all statements in the method body
    StatementValidator stmtValidator(reporter);
    std::unordered_map<std::string, FunctionInfo> emptyFunctionTable;
    
    bool hasErrors = false;
    for (const auto &stmt : method.body) {
        if (!stmtValidator.validateStatement(stmt.get(), symbolTable, 
                                           emptyFunctionTable,
                                           structTable,
                                           classTable, nonNilVars,
                                           currentFunction, currentClass)) {
            hasErrors = true;
        }
    }
    
    // additional constructor validation
    if (isConstructor) {
        for (const auto &stmt : method.body) {
            if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
                if (ret->value) {
                    reporter.reportError("Constructor cannot return a value", ret->line);
                    hasErrors = true;
                }
            }
        }
    }
    
    // restore symbol table and function context
    symbolTable = savedSymbolTable;
    nonNilVars = savedNonNilVars;
    currentFunction = savedFunction;
    currentClass = savedCurrentClass;
    return !hasErrors;
}

bool ClassValidator::checkClassFieldsInitialized(const ClassDecl *classDecl) {
    if (!classDecl || !classDecl->constructor) {
        return true;
    }

    const ClassMethod *constructor = classDecl->constructor.get();
    
    if (!constructor) {
        return true;
    }

    std::set<std::string> initializedFields;

    // check for field assignments in the constructor body
    for (const auto &stmt : constructor->body) {
        if (auto *fieldAssign = dynamic_cast<const FieldAssignment *>(stmt.get())) {
            if (dynamic_cast<const SelfExpr *>(fieldAssign->object.get())) {
                initializedFields.insert(fieldAssign->fieldName);
            }
        }
    }

    // check which public fields are not initialized
    for (const auto &field : classDecl->fields) {
        if (field.visibility == Visibility::PUBLIC && !field.isStatic) {
            bool hasDefault = field.hasDefault;
            bool isInitialized = initializedFields.count(field.name) > 0;
            
            if (!hasDefault && !isInitialized) {
                reporter.reportError("Public field '" + field.name + "' of class '" + classDecl->name + 
                                  "' is not initialized in the constructor. Add 'self." + field.name + 
                                  " = <value>' in __init", classDecl->line);
                return false;
            }
        }
    }

    return true;
}

bool ClassValidator::validateMethodCall(const std::string &className,
                                       const std::string &methodName,
                                       const std::map<std::string, ClassInfo> &classTable,
                                       const std::string &currentClass,
                                       int line) {
    if (!classTable.count(className)) {
        reporter.reportError("Class '" + className + "' is not defined", line);
        return false;
    }

    const auto &classInfo = classTable.at(className);
    
    if (!classInfo.methodInfo.count(methodName)) {
        reporter.reportError("Method '" + methodName + "' does not exist in class '" + className + "'", line);
        return false;
    }

    // get method visibility
    auto methodInfoPair = classInfo.methodInfo.at(methodName);
    Visibility methodVisibility = methodInfoPair.second;
    
    if (methodName != "__init") {
        if (methodVisibility == Visibility::PRIVATE) {
            bool isInsideSameClass = false;

            if (currentClass == className) {
                isInsideSameClass = true;
            }
            
            if (!isInsideSameClass) {
                reporter.reportError("Cannot call private method '" + methodName + 
                                  "' from outside class '" + className + "'", line);
                return false;
            }
        }
    }

    return true;
}

bool ClassValidator::validateFieldAccess(const std::string &className,
                                        const std::string &fieldName,
                                        const std::map<std::string, ClassInfo> &classTable,
                                        const std::string &currentClass,
                                        int line) {
    if (!classTable.count(className)) {
        reporter.reportError("Class '" + className + "' is not defined", line);
        return false;
    }

    const auto &classInfo = classTable.at(className);
    
    // check if field exists
    for (const auto &field : classInfo.fields) {
        if (field.name == fieldName) {
            if (field.visibility == Visibility::PRIVATE && currentClass != className) {
                reporter.reportError("Cannot access private field '" + fieldName + 
                                  "' from outside class '" + className + "'", line);
                return false;
            }
            return true;
        }
    }

    reporter.reportError("Class '" + className + "' has no field '" + fieldName + "'", line);
    return false;
}

bool ClassValidator::validateConstructor(const ClassDecl *decl,
                                       const ClassMethod *constructor) {
    if (!constructor)
        return true;

    std::set<std::string> fieldNames;
    for (const auto &field : decl->fields) {
        fieldNames.insert(field.name);
    }

    return true;
}

} // namespace HolyLua