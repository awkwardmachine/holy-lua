#pragma once
#include "../../ast.h"
#include "../../utils/type_utils.h"
#include "../../utils/error_reporter.h"
#include "expr_validator.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <unordered_set>

namespace HolyLua {

class StatementValidator {
public:
    StatementValidator(ErrorReporter &reporter);
    
    bool validateStatement(const ASTNode *node,
                          std::unordered_map<std::string, TypeInfo> &symbolTable,
                          const std::unordered_map<std::string, FunctionInfo> &functionTable,
                          const std::map<std::string, StructInfo> &structTable,
                          const std::map<std::string, ClassInfo> &classTable,
                          std::unordered_set<std::string> &nonNilVars,
                          std::string &currentFunction,
                          std::string &currentClass);
    
private:
    ErrorReporter &reporter;
    ExpressionValidator exprValidator;
    
    bool validateVarDecl(const VarDecl *decl,
                        std::unordered_map<std::string, TypeInfo> &symbolTable,
                        const std::map<std::string, StructInfo> &structTable,
                        const std::map<std::string, ClassInfo> &classTable);
    
    bool validateAssignment(const Assignment *assign,
                           std::unordered_map<std::string, TypeInfo> &symbolTable,
                           std::unordered_set<std::string> &nonNilVars);
    
    bool validateFieldAssignment(const FieldAssignment *assign,
                                std::unordered_map<std::string, TypeInfo> &symbolTable,
                                const std::map<std::string, StructInfo> &structTable,
                                const std::map<std::string, ClassInfo> &classTable,
                                const std::string &currentClass);
    
    bool validatePrintStmt(const PrintStmt *print,
                      const std::unordered_map<std::string, TypeInfo> &symbolTable,
                      const std::unordered_set<std::string> &nonNilVars,
                      const std::map<std::string, StructInfo> &structTable,
                      const std::map<std::string, ClassInfo> &classTable,
                      const std::string &currentClass);
    
    bool validateIfStmt(const IfStmt *ifStmt,
                       std::unordered_map<std::string, TypeInfo> &symbolTable,
                       const std::unordered_map<std::string, FunctionInfo> &functionTable,
                       const std::map<std::string, StructInfo> &structTable,
                       const std::map<std::string, ClassInfo> &classTable,
                       std::unordered_set<std::string> &nonNilVars,
                       std::string &currentFunction,
                       std::string &currentClass);
    
    bool validateReturnStmt(const ReturnStmt *ret,
                          const std::unordered_map<std::string, TypeInfo> &symbolTable,
                          const std::unordered_map<std::string, FunctionInfo> &functionTable,
                          const std::map<std::string, StructInfo> &structTable,
                          const std::map<std::string, ClassInfo> &classTable,
                          const std::string &currentClass);
};

} // namespace HolyLua