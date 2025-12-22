#pragma once
#include "../ast.h"
#include "../common.h"
#include <string>

namespace HolyLua {

struct UsageConstraint {
    ValueType requiredType;
    int line;
    std::string context;

    UsageConstraint(ValueType t, int l, std::string c)
        : requiredType(t), line(l), context(std::move(c)) {}
};

struct ReturnAnalysis {
    std::vector<ValueType> returnTypes;
    std::vector<int> returnLines;
    bool hasConflict;
    ValueType inferredType;

    ReturnAnalysis() : hasConflict(false), inferredType(ValueType::INFERRED) {}
};

class TypeUtils {
public:
    static std::string typeToString(ValueType type);
    static bool isCompatible(ValueType expected, ValueType actual);
    static std::string binaryOpToString(BinaryOp op);
    static bool operatorRequiresType(BinaryOp op, ValueType &requiredType);
    static ValueType resolveTypeName(const std::string &typeName);
};

} // namespace HolyLua