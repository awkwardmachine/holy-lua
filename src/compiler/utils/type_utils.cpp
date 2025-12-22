#include "../../../include/compiler/compiler.h"
#include <cstdio>

namespace HolyLua {

std::string Compiler::getCTypeForStruct(const std::string &structName) {
  return structName;
}

std::string Compiler::getCTypeWithOptional(ValueType type, bool isOptional) {
  std::string ctype = getCType(type);
  if (isOptional) {
    return ctype;
  }
  return ctype;
}

std::string Compiler::getCType(ValueType type, const std::string &structTypeName) {
  switch (type) {
  case ValueType::NUMBER:
    return "double";
  case ValueType::STRING:
    return "char*";
  case ValueType::BOOL:
    return "int";
  case ValueType::INFERRED:
    return "double";
  case ValueType::FUNCTION:
    return "void*";
  case ValueType::STRUCT:
    if (!structTypeName.empty()) {
      return structTypeName;
    }
    return "void*";
  case ValueType::ENUM:
    if (!structTypeName.empty()) {
      return structTypeName;
    }
    return "int";
  default:
    return "void";
  }
}

std::string Compiler::getCTypeForVar(const std::string &varName) {
  if (symbolTable.count(varName)) {
    auto &varInfo = symbolTable[varName];
    if (varInfo.type == ValueType::STRUCT && varInfo.isOptional) {
      return "double";
    }
    return getCType(varInfo.type, varInfo.structTypeName);
  }
  return "double";
}

std::string Compiler::typeToString(ValueType type) {
  switch (type) {
  case ValueType::NUMBER:
    return "number";
  case ValueType::STRING:
    return "string";
  case ValueType::BOOL:
    return "bool";
  case ValueType::INFERRED:
    return "number";
  case ValueType::FUNCTION:
    return "function";
  case ValueType::STRUCT:
    return "struct";
  case ValueType::ENUM:
    return "enum";
  default:
    return "number";
  }
}

std::string Compiler::formatString(ValueType type) {
  switch (type) {
  case ValueType::NUMBER:
    return "%g";
  case ValueType::STRING:
    return "%s";
  case ValueType::BOOL:
    return "%d";
  case ValueType::INFERRED:
    return "%g";
  case ValueType::ENUM:
    return "%d";
  default:
    return "%s";
  }
}

ValueType Compiler::inferType(
    const std::variant<int64_t, double, std::string, bool> &value) {
  if (std::holds_alternative<int64_t>(value)) {
    return ValueType::NUMBER;
  } else if (std::holds_alternative<double>(value)) {
    return ValueType::NUMBER;
  } else if (std::holds_alternative<std::string>(value)) {
    return ValueType::STRING;
  } else if (std::holds_alternative<bool>(value)) {
    return ValueType::BOOL;
  }
  return ValueType::INFERRED;
}

std::string Compiler::doubleToString(double value) {
  char buffer[64];
  
  snprintf(buffer, sizeof(buffer), "%.9f", value);
  
  std::string result = buffer;
  
  // remove trailing zeros
  size_t dot = result.find('.');
  if (dot != std::string::npos) {
    while (!result.empty() && result.back() == '0') {
      result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
      result.pop_back();
    }
  }
  
  return result;
}

std::string Compiler::valueToString(
    const std::variant<int64_t, double, std::string, bool> &value) {
  if (auto *i = std::get_if<int64_t>(&value)) {
    return std::to_string(*i) + ".0";
  } else if (auto *d = std::get_if<double>(&value)) {
    return doubleToString(*d);
  } else if (auto *s = std::get_if<std::string>(&value)) {
    return "\"" + *s + "\"";
  } else if (auto *b = std::get_if<bool>(&value)) {
    return *b ? "1" : "0";
  }
  return "0.0";
}

bool Compiler::isStringExpr(const Expr *expr) {
  if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    if (call->name == "tostring")
      return true;
    return false;
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (bin->op == BinaryOp::CONCAT)
      return true;
    return isStringExpr(bin->left.get()) || isStringExpr(bin->right.get());
  } else if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
    return std::holds_alternative<std::string>(lit->value);
  } else if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (symbolTable.count(var->name)) {
      return symbolTable[var->name].type == ValueType::STRING;
    }
  }
  return false;
}

bool Compiler::isNumberExpr(const Expr *expr) {
  if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
    return std::holds_alternative<int64_t>(lit->value) ||
           std::holds_alternative<double>(lit->value);
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (bin->op == BinaryOp::ADD || bin->op == BinaryOp::SUBTRACT ||
        bin->op == BinaryOp::MULTIPLY || bin->op == BinaryOp::DIVIDE ||
        bin->op == BinaryOp::MODULO || bin->op == BinaryOp::POWER ||
        bin->op == BinaryOp::FLOOR_DIVIDE) {
      return true;
    }
  }
  return false;
}

bool Compiler::isBoolExpr(const Expr *expr) {
  if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
    return std::holds_alternative<bool>(lit->value);
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (bin->op == BinaryOp::EQUAL || bin->op == BinaryOp::NOT_EQUAL ||
        bin->op == BinaryOp::LESS || bin->op == BinaryOp::LESS_EQUAL ||
        bin->op == BinaryOp::GREATER || bin->op == BinaryOp::GREATER_EQUAL) {
      return true;
    }
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    return un->op == UnaryOp::NOT;
  }
  return false;
}

} // namespace HolyLua