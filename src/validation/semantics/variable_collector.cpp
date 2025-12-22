#include "../../../include/validation/semantics/variable_collector.h"

namespace HolyLua {

VariableCollector::VariableCollector(ErrorReporter &reporter) 
    : reporter(reporter) {}

bool VariableCollector::collectGlobalVariables(const Program &program,
                                             std::unordered_map<std::string, TypeInfo> &symbolTable,
                                             const std::map<std::string, StructInfo> &structTable,
                                             const std::map<std::string, ClassInfo> &classTable) {
    for (auto &stmt : program.statements) {
        if (!stmt)
            continue;

        if (auto *decl = dynamic_cast<VarDecl *>(stmt.get())) {
            if (!processVariableDeclaration(decl, symbolTable, structTable, classTable)) {
                return false;
            }
        }
    }

    return true;
}

void VariableCollector::collectLocalVariables(const std::vector<std::unique_ptr<ASTNode>> &stmts,
                                            std::unordered_map<std::string, TypeInfo> &symbolTable,
                                            const std::map<std::string, StructInfo> &structTable,
                                            const std::map<std::string, ClassInfo> &classTable) {
    for (const auto &stmt : stmts) {
        if (!stmt) continue;

        if (auto *decl = dynamic_cast<const VarDecl *>(stmt.get())) {
            if (decl->isGlobal) {
                continue;
            }

            ValueType type = decl->type;
            bool isFunction = false;
            bool isStruct = false;
            std::string structTypeName = decl->typeName;

            if (type == ValueType::INFERRED && decl->hasValue) {
                if (auto *lit = dynamic_cast<const LiteralExpr *>(decl->value.get())) {
                    if (std::holds_alternative<int64_t>(lit->value) || 
                        std::holds_alternative<double>(lit->value)) {
                        type = ValueType::NUMBER;
                    } else if (std::holds_alternative<std::string>(lit->value)) {
                        type = ValueType::STRING;
                    } else if (std::holds_alternative<bool>(lit->value)) {
                        type = ValueType::BOOL;
                    }
                } else if (dynamic_cast<const LambdaExpr *>(decl->value.get())) {
                    type = ValueType::FUNCTION;
                    isFunction = true;
                } else if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
                    type = ValueType::STRUCT;
                    isStruct = true;
                    if (structTypeName.empty()) {
                        structTypeName = structCons->structName;
                    }
                } else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
                    type = ValueType::STRUCT;
                    isStruct = true;
                    if (structTypeName.empty()) {
                        structTypeName = classInst->className;
                    }
                } else {
                    type = ValueType::NUMBER;
                }
            }

            symbolTable[decl->name] = {type, decl->isConst, true, decl->isOptional,
                                     isFunction, isStruct, structTypeName};
        }
        // recurse into control structures
        else if (auto *ifStmt = dynamic_cast<const IfStmt *>(stmt.get())) {
            collectLocalVariables(ifStmt->thenBlock, symbolTable, structTable, classTable);
            for (auto &branch : ifStmt->elseifBranches) {
                collectLocalVariables(branch.second, symbolTable, structTable, classTable);
            }
            collectLocalVariables(ifStmt->elseBlock, symbolTable, structTable, classTable);
        }
        else if (auto *whileStmt = dynamic_cast<const WhileStmt *>(stmt.get())) {
            collectLocalVariables(whileStmt->body, symbolTable, structTable, classTable);
        }
        else if (auto *forStmt = dynamic_cast<const ForStmt *>(stmt.get())) {
            // add loop variable if not already present
            if (symbolTable.find(forStmt->varName) == symbolTable.end()) {
                symbolTable[forStmt->varName] = {ValueType::NUMBER, false, true, false,
                                               false, false, ""};
            }
            
            collectLocalVariables(forStmt->body, symbolTable, structTable, classTable);
        }
        else if (auto *repeatStmt = dynamic_cast<const RepeatStmt *>(stmt.get())) {
            collectLocalVariables(repeatStmt->body, symbolTable, structTable, classTable);
        }
    }
}

bool VariableCollector::processVariableDeclaration(const VarDecl *decl,
                                                 std::unordered_map<std::string, TypeInfo> &symbolTable,
                                                 const std::map<std::string, StructInfo> &structTable,
                                                 const std::map<std::string, ClassInfo> &classTable) {
    if (symbolTable.count(decl->name)) {
        reporter.reportError("Variable '" + decl->name + "' is already declared", decl->line);
        return false;
    }

    ValueType declaredType = decl->type;
    bool isOptional = decl->isOptional;
    bool isFunction = false;
    bool isStruct = false;
    std::string structTypeName = "";

    // check if type is a struct or class from the type annotation
    if (!decl->typeName.empty()) {
        if (structTable.count(decl->typeName)) {
            isStruct = true;
            declaredType = ValueType::STRUCT;
            structTypeName = decl->typeName;
        } else if (classTable.count(decl->typeName)) {
            isStruct = true;
            declaredType = ValueType::STRUCT;
            structTypeName = decl->typeName;
        } else if (decl->type == ValueType::STRUCT || 
                  (!decl->typeName.empty() && decl->typeName != "number" && 
                   decl->typeName != "string" && decl->typeName != "bool" && 
                   decl->typeName != "function" && decl->typeName != "struct")) {
            reporter.reportError("Unknown type '" + decl->typeName + "' for variable '" + 
                               decl->name + "'", decl->line);
            return false;
        }
    }

    if (decl->hasValue && decl->value) {
        // check if it's a lambda
        if (dynamic_cast<const LambdaExpr *>(decl->value.get())) {
            isFunction = true;
            declaredType = ValueType::FUNCTION;

            if (decl->type != ValueType::INFERRED &&
                decl->type != ValueType::FUNCTION) {
                reporter.reportError("Cannot assign function to variable of type " +
                                    TypeUtils::typeToString(decl->type), decl->line);
                return false;
            }
        }
        // check if it's a struct constructor
        else if (auto *structCons = dynamic_cast<const StructConstructor *>(
                     decl->value.get())) {
            isStruct = true;
            declaredType = ValueType::STRUCT;

            if (structTypeName.empty()) {
                structTypeName = structCons->structName;
            }

            if (!structTable.count(structCons->structName) && 
                !classTable.count(structCons->structName)) {
                reporter.reportError("Struct/Class '" + structCons->structName + "' is not defined",
                                   decl->line);
                return false;
            }

            // validate that declared type matches
            if (decl->type != ValueType::INFERRED && !decl->typeName.empty()) {
                if (decl->typeName != structCons->structName) {
                    reporter.reportError("Type mismatch: variable declared as '" + decl->typeName +
                                        "' but initialized with '" + structCons->structName +
                                        "'", decl->line);
                    return false;
                }
            }
        }
        // check if it's a class instantiation
        else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(
                     decl->value.get())) {
            isStruct = true;
            declaredType = ValueType::STRUCT;

            if (structTypeName.empty()) {
                structTypeName = classInst->className;
            }

            if (!classTable.count(classInst->className)) {
                reporter.reportError("Class '" + classInst->className + "' is not defined",
                                   classInst->line);
                return false;
            }

            // validate that declared type matches
            if (decl->type != ValueType::INFERRED && !decl->typeName.empty()) {
                if (decl->typeName != classInst->className) {
                    reporter.reportError("Type mismatch: variable declared as '" + decl->typeName +
                                        "' but initialized with '" + classInst->className +
                                        "'", decl->line);
                    return false;
                }
            }
        }
    } else {
        if (declaredType == ValueType::INFERRED && !isStruct) {
            reporter.reportError("Variable '" + decl->name +
                                "' must be initialized or have an explicit type",
                              decl->line);
            return false;
        }

        if (!isOptional && !isStruct) {
            reporter.reportError(
                "Non-optional variable '" + decl->name +
                "' must be initialized. " +
                "Either provide a value or use optional type (e.g., 'local " +
                decl->name + ": " + TypeUtils::typeToString(declaredType) + "?')",
                decl->line);
            return false;
        }
    }

    // add to symbol table
    symbolTable[decl->name] = {declaredType,  decl->isConst, true,
                             isOptional,    isFunction,    isStruct,
                             structTypeName};
    return true;
}

} // namespace HolyLua