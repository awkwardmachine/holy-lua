#pragma once
#include "../../ast.h"
#include "../../utils/type_utils.h"
#include "../../utils/error_reporter.h"
#include "../../common.h"
#include <unordered_map>
#include <vector>
#include <memory>

namespace HolyLua {

class VariableCollector {
public:
    VariableCollector(ErrorReporter &reporter);
    
    bool collectGlobalVariables(const Program &program,
                               std::unordered_map<std::string, TypeInfo> &symbolTable,
                               const std::map<std::string, StructInfo> &structTable,
                               const std::map<std::string, ClassInfo> &classTable);
    
    void collectLocalVariables(const std::vector<std::unique_ptr<ASTNode>> &stmts,
                              std::unordered_map<std::string, TypeInfo> &symbolTable,
                              const std::map<std::string, StructInfo> &structTable,
                              const std::map<std::string, ClassInfo> &classTable);
    
private:
    ErrorReporter &reporter;
    
    bool processVariableDeclaration(const VarDecl *decl,
                                   std::unordered_map<std::string, TypeInfo> &symbolTable,
                                   const std::map<std::string, StructInfo> &structTable,
                                   const std::map<std::string, ClassInfo> &classTable);
};

} // namespace HolyLua