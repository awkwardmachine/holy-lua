#pragma once
#include "../../ast.h"
#include "../../utils/error_reporter.h"
#include "../../common.h"
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace HolyLua {

class ClassValidator {
public:
    ClassValidator(ErrorReporter &reporter);
    
    bool collectClassDeclarations(const Program &program,
                                 std::map<std::string, ClassInfo> &classTable);
    
    bool validateClassDeclaration(const ClassDecl *decl,
                                            std::map<std::string, ClassInfo> &classTable,
                                            const std::map<std::string, StructInfo> &structTable);
    
    bool validateClassMethod(const std::string &className,
                            const ClassMethod &method,
                            bool isConstructor,
                            std::unordered_map<std::string, TypeInfo> &symbolTable,
                            const std::map<std::string, StructInfo> &structTable,
                            std::map<std::string, ClassInfo> &classTable,
                            std::unordered_set<std::string> &nonNilVars,
                            std::string &currentClass,
                            std::string &currentFunction);
    
    bool checkClassFieldsInitialized(const ClassDecl *classDecl);
    
    bool validateMethodCall(const std::string &className,
                           const std::string &methodName,
                           const std::map<std::string, ClassInfo> &classTable,
                           const std::string &currentClass,
                           int line);
    
    bool validateFieldAccess(const std::string &className,
                            const std::string &fieldName,
                            const std::map<std::string, ClassInfo> &classTable,
                            const std::string &currentClass,
                            int line);
    
private:
    ErrorReporter &reporter;
    
    bool validateConstructor(const ClassDecl *decl,
                            const ClassMethod *constructor);
};

} // namespace HolyLua