#include "../../include/utils/type_utils.h"

namespace HolyLua {

std::string TypeUtils::typeToString(ValueType type) {
    switch (type) {
    case ValueType::NUMBER:
        return "number";
    case ValueType::STRING:
        return "string";
    case ValueType::BOOL:
        return "bool";
    case ValueType::FUNCTION:
        return "function";
    case ValueType::STRUCT:
        return "struct";
    case ValueType::INFERRED:
        return "inferred";
    default:
        return "unknown";
    }
}

bool TypeUtils::isCompatible(ValueType expected, ValueType actual) {
    if (expected == ValueType::INFERRED || actual == ValueType::INFERRED) {
        return true;
    }

    if (expected == ValueType::STRUCT && actual == ValueType::STRUCT) {
        return true;
    }

    return expected == actual;
}

std::string TypeUtils::binaryOpToString(BinaryOp op) {
    switch (op) {
    case BinaryOp::ADD:
        return "+";
    case BinaryOp::SUBTRACT:
        return "-";
    case BinaryOp::MULTIPLY:
        return "*";
    case BinaryOp::DIVIDE:
        return "/";
    case BinaryOp::MODULO:
        return "%";
    case BinaryOp::CONCAT:
        return "..";
    case BinaryOp::EQUAL:
        return "==";
    case BinaryOp::NOT_EQUAL:
        return "!=";
    case BinaryOp::LESS:
        return "<";
    case BinaryOp::LESS_EQUAL:
        return "<=";
    case BinaryOp::GREATER:
        return ">";
    case BinaryOp::GREATER_EQUAL:
        return ">=";
    case BinaryOp::NIL_COALESCE:
        return "??";
    default:
        return "unknown";
    }
}

bool TypeUtils::operatorRequiresType(BinaryOp op, ValueType &requiredType) {
    switch (op) {
    case BinaryOp::ADD:
    case BinaryOp::SUBTRACT:
    case BinaryOp::MULTIPLY:
    case BinaryOp::DIVIDE:
    case BinaryOp::MODULO:
        requiredType = ValueType::NUMBER;
        return true;

    case BinaryOp::CONCAT:
        requiredType = ValueType::STRING;
        return true;

    default:
        return false;
    }
}

ValueType TypeUtils::resolveTypeName(const std::string &typeName) {
    if (typeName == "number")
        return ValueType::NUMBER;
    if (typeName == "string")
        return ValueType::STRING;
    if (typeName == "bool")
        return ValueType::BOOL;
    if (typeName == "function")
        return ValueType::FUNCTION;
    return ValueType::INFERRED;
}

} // namespace HolyLua