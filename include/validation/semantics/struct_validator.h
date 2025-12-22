#pragma once
#include "../../ast.h"
#include "../../utils/error_reporter.h"
#include "../../common.h"
#include <map>
#include <string>
#include <unordered_map>

namespace HolyLua {

class StructValidator {
public:
    StructValidator(ErrorReporter &reporter);
    
    bool collectStructDeclarations(const Program &program,
                                  std::map<std::string, StructInfo> &structTable);
    
    bool validateStructFieldAccess(const std::string &structName,
                                  const std::string &fieldName,
                                  const std::map<std::string, StructInfo> &structTable,
                                  int line);
    
    std::string getFieldStructType(const FieldAccessExpr *field,
                                  const std::unordered_map<std::string, TypeInfo> &symbolTable,
                                  const std::map<std::string, StructInfo> &structTable,
                                  const std::string &currentClass);
    
private:
    ErrorReporter &reporter;
};

} // namespace HolyLua