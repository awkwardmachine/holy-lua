#include "../../include/validation/type_checker.h"

namespace HolyLua {

TypeChecker::TypeChecker(const std::string &source)
    : reporter(source),
      functionValidator(reporter),
      structValidator(reporter),
      classValidator(reporter),
      variableCollector(reporter),
      statementValidator(reporter) {}

void TypeChecker::initBuiltinFunctions() {
    // tostring: converts any value to string
    FunctionInfo tostringInfo;
    tostringInfo.name = "tostring";
    tostringInfo.parameters = {{"value", ValueType::INFERRED}};
    tostringInfo.parameterOptionals = {false};
    tostringInfo.returnType = ValueType::STRING;
    tostringInfo.isGlobal = true;
    tostringInfo.nestedFunctions = {};
    functionTable["tostring"] = tostringInfo;

    // print: prints any value
    FunctionInfo printInfo;
    printInfo.name = "print";
    printInfo.parameters = {{"value", ValueType::INFERRED}};
    printInfo.parameterOptionals = {false};
    printInfo.returnType = ValueType::INFERRED;
    printInfo.isGlobal = true;
    printInfo.nestedFunctions = {};
    functionTable["print"] = printInfo;

    // tonumber: converts string to number
    FunctionInfo tonumberInfo;
    tonumberInfo.name = "tonumber";
    tonumberInfo.parameters = {{"value", ValueType::STRING}};
    tonumberInfo.parameterOptionals = {false};
    tonumberInfo.returnType = ValueType::NUMBER;
    tonumberInfo.isGlobal = true;
    tonumberInfo.nestedFunctions = {};
    functionTable["tonumber"] = tonumberInfo;

    // type: returns the type of a value as string
    FunctionInfo typeInfo;
    typeInfo.name = "type";
    typeInfo.parameters = {{"value", ValueType::INFERRED}};
    typeInfo.parameterOptionals = {false};
    typeInfo.returnType = ValueType::STRING;
    typeInfo.isGlobal = true;
    typeInfo.nestedFunctions = {};
    functionTable["type"] = typeInfo;
}

bool TypeChecker::check(const Program &program) {
    initBuiltinFunctions();
    
    if (!performFirstPass(program)) return false;
    if (!performSecondPass(program)) return false;
    if (!performThirdPass(program)) return false;
    if (!performFourthPass(program)) return false;
    
    return !reporter.hasErrors();
}

bool TypeChecker::performFirstPass(const Program &program) {
    if (!structValidator.collectStructDeclarations(program, structTable)) {
        return false;
    }
    
    if (!classValidator.collectClassDeclarations(program, classTable)) {
        return false;
    }
    
    // validate struct declarations
    for (auto &stmt : program.statements) {
        if (!stmt) continue;
        if (auto *structDecl = dynamic_cast<StructDecl *>(stmt.get())) {
            std::set<std::string> fieldNames;
            for (const auto &field : structDecl->fields) {
                if (fieldNames.count(field.name)) {
                    reporter.reportError("Duplicate field name '" + field.name + 
                                       "' in struct '" + structDecl->name + "'", structDecl->line);
                    return false;
                }
                fieldNames.insert(field.name);
            }
        }
    }
    
    for (auto &stmt : program.statements) {
        if (!stmt) continue;
        if (auto *classDecl = dynamic_cast<ClassDecl *>(stmt.get())) {
            if (!classValidator.validateClassDeclaration(classDecl, classTable, structTable)) {
                return false;
            }
        }
    }
    
    bool success = variableCollector.collectGlobalVariables(program, symbolTable, structTable, classTable);
    return success;
}

bool TypeChecker::performSecondPass(const Program &program) {
    // collect function signatures
    for (auto &stmt : program.statements) {
        if (!stmt) continue;
        if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
            if (!functionValidator.collectFunctionSignature(func, functionTable)) {
                return false;
            }
        }
    }
    return true;
}

bool TypeChecker::performThirdPass(const Program &program) {
    // infer types and validate functions
    for (auto &stmt : program.statements) {
        if (!stmt) continue;
        if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
            if (!functionValidator.inferAndValidateFunction(func, symbolTable, 
                                                           functionTable, variableCollector)) {
                return false;
            }
        }
    }
    
    for (auto &stmt : program.statements) {
        if (!stmt) continue;
        if (auto *classDecl = dynamic_cast<ClassDecl *>(stmt.get())) {
            // validate constructor
            if (classDecl->constructor) {
                if (!classValidator.validateClassMethod(classDecl->name, 
                                                       *classDecl->constructor.get(),
                                                       true,
                                                       symbolTable,
                                                       structTable,
                                                       classTable,
                                                       nonNilVars,
                                                       currentClass,
                                                       currentFunction)) {
                    return false;
                }
            }
            
            // validate methods
            for (const auto &method : classDecl->methods) {
                if (!classValidator.validateClassMethod(classDecl->name,
                                                       method,
                                                       false,
                                                       symbolTable,
                                                       structTable,
                                                       classTable,
                                                       nonNilVars,
                                                       currentClass,
                                                       currentFunction)) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

bool TypeChecker::performFourthPass(const Program &program) {
    // check all statements
    for (const auto &stmt : program.statements) {
        if (!stmt) continue;

        if (dynamic_cast<const VarDecl *>(stmt.get())) {
            continue;
        }

        if (dynamic_cast<const ClassDecl *>(stmt.get())) {
            continue;
        }

        if (dynamic_cast<const StructDecl *>(stmt.get())) {
            continue;
        }
        
        if (!statementValidator.validateStatement(stmt.get(), symbolTable, functionTable,
                                                 structTable, classTable, nonNilVars,
                                                 currentFunction, currentClass)) {
            return false;
        }
    }
    return true;
}

} // namespace HolyLua