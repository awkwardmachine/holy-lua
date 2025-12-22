#pragma once
#include "../../ast.h"
#include "../../utils/type_utils.h"
#include "../../utils/error_reporter.h"
#include <unordered_map>
#include <string>

namespace HolyLua {

class TypeCompatibility {
public:
    static bool checkTypeCompatibility(ValueType expected, ValueType actual, 
                                      const std::string &context, int line,
                                      ErrorReporter &reporter);
    
    static bool checkAssignment(const std::string &varName, 
                               const TypeInfo &varInfo,
                               ValueType valueType, bool valueCanBeNil,
                               int line, ErrorReporter &reporter);
    
    static bool checkCompoundAssignment(const std::string &varName,
                                       const TypeInfo &varInfo,
                                       ValueType valueType, int line,
                                       ErrorReporter &reporter);
};

} // namespace HolyLua