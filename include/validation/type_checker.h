#pragma once
#include "../ast.h"
#include "../common.h"
#include "../utils/error_reporter.h"
#include "semantics/function_validator.h"
#include "semantics/struct_validator.h"
#include "semantics/class_validator.h"
#include "semantics/variable_collector.h"
#include "ast_validation/stmt_validator.h"
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace HolyLua {

class TypeChecker {
public:
    TypeChecker(const std::string &source);
    bool check(const Program &program);
    bool hasErrors() const { return reporter.hasErrors(); }
    
private:
    ErrorReporter reporter;

    FunctionValidator functionValidator;
    StructValidator structValidator;
    ClassValidator classValidator;
    VariableCollector variableCollector;
    StatementValidator statementValidator;
    
    std::unordered_map<std::string, TypeInfo> symbolTable;
    std::unordered_map<std::string, FunctionInfo> functionTable;
    std::unordered_set<std::string> nonNilVars;
    std::map<std::string, StructInfo> structTable;
    std::map<std::string, ClassInfo> classTable;
    
    std::string currentFunction;
    std::string currentClass;
    
    void initBuiltinFunctions();
    bool performFirstPass(const Program &program);
    bool performSecondPass(const Program &program);
    bool performThirdPass(const Program &program);
    bool performFourthPass(const Program &program);
};

} // namespace HolyLua