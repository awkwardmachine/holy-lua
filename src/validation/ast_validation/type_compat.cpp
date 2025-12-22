#include "../../../include/validation/ast_validation/type_compat.h"

namespace HolyLua {

bool TypeCompatibility::checkTypeCompatibility(ValueType expected, ValueType actual, 
                                              const std::string &context, int line,
                                              ErrorReporter &reporter) {
    if (!TypeUtils::isCompatible(expected, actual)) {
        reporter.reportError("Type mismatch in " + context + ": expected " + 
                           TypeUtils::typeToString(expected) + " but got " +
                           TypeUtils::typeToString(actual), line);
        return false;
    }
    return true;
}

bool TypeCompatibility::checkAssignment(const std::string &varName, 
                                       const TypeInfo &varInfo,
                                       ValueType valueType, bool valueCanBeNil,
                                       int line, ErrorReporter &reporter) {
    if (varInfo.isOptional) {
        if (!valueCanBeNil && !TypeUtils::isCompatible(varInfo.type, valueType)) {
            reporter.reportError("Type mismatch: cannot assign " + 
                               TypeUtils::typeToString(valueType) +
                               " to variable '" + varName + "' of type " +
                               TypeUtils::typeToString(varInfo.type) + "?", line);
            return false;
        }
    } else {
        if (valueCanBeNil) {
            reporter.reportError("Cannot assign nil to non-optional variable '" + 
                               varName + "'", line);
            return false;
        }
        if (!TypeUtils::isCompatible(varInfo.type, valueType)) {
            reporter.reportError("Type mismatch: cannot assign " + 
                               TypeUtils::typeToString(valueType) +
                               " to variable '" + varName + "' of type " +
                               TypeUtils::typeToString(varInfo.type), line);
            return false;
        }
    }
    return true;
}

bool TypeCompatibility::checkCompoundAssignment(const std::string &varName,
                                               const TypeInfo &varInfo,
                                               ValueType valueType, int line,
                                               ErrorReporter &reporter) {
    if (varInfo.type != ValueType::NUMBER || valueType != ValueType::NUMBER) {
        reporter.reportError("Compound assignment on variable '" + varName + 
                           "' requires number types, but got " +
                           TypeUtils::typeToString(varInfo.type) + " and " +
                           TypeUtils::typeToString(valueType), line);
        return false;
    }
    return true;
}

} // namespace HolyLua