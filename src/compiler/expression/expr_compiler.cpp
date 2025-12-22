#include "../../../include/compiler/compiler.h"

namespace HolyLua {

std::string Compiler::compileExpr(const Expr *expr, ValueType expectedType,
                                  bool forGlobalInit) {
  if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
    return valueToString(lit->value);
  } else if (dynamic_cast<const NilExpr *>(expr)) {
    if (expectedType == ValueType::STRING) {
      return "NULL";
    } else if (expectedType == ValueType::NUMBER) {
      return "HL_NIL_NUMBER";
    } else if (expectedType == ValueType::BOOL) {
      return "-1";
    } else if (expectedType == ValueType::ENUM) {
      return "-1";
    } else if (expectedType == ValueType::STRUCT) {
      return "HL_NIL_NUMBER";
    } else {
      return "HL_NIL_NUMBER";
    }
  } else if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    return var->name;
  } else if (auto *enumAccess = dynamic_cast<const EnumAccessExpr *>(expr)) {
    return compileEnumAccess(enumAccess);
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    return compileFunctionCall(call);
  } else if (auto *methodCall = dynamic_cast<const MethodCall *>(expr)) {
    std::string result = compileMethodCall(methodCall);
    if (result.empty()) {
      return "0";
    }
    return result;
  } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    return compileExpr(unwrap->operand.get(), expectedType, forGlobalInit);
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (bin->op == BinaryOp::NIL_COALESCE) {
      std::string left =
          compileExpr(bin->left.get(), expectedType, forGlobalInit);
      std::string right =
          compileExpr(bin->right.get(), expectedType, forGlobalInit);
      ValueType leftType = inferExprType(bin->left.get());

      if (leftType == ValueType::STRING) {
        return "((" + left + ") == NULL ? (" + right + ") : (" + left + "))";
      } else if (leftType == ValueType::NUMBER) {
        return "(isnan(" + left + ") ? (" + right + ") : (" + left + "))";
      } else if (leftType == ValueType::ENUM) {
        return "((" + left + ") == -1 ? (" + right + ") : (" + left + "))";
      } else if (leftType == ValueType::STRUCT) {
        return "(isnan(" + left + ") ? (" + right + ") : (" + left + "))";
      } else {
        return "((" + left + ") == -1 ? (" + right + ") : (" + left + "))";
      }
    }

    if (bin->op == BinaryOp::CONCAT) {
      std::string leftStr = compileExprForConcat(bin->left.get());
      std::string rightStr = compileExprForConcat(bin->right.get());
      return "hl_concat_strings(" + leftStr + ", " + rightStr + ")";
    }

    if (bin->op == BinaryOp::POWER) {
      std::string left =
          compileExpr(bin->left.get(), expectedType, forGlobalInit);
      std::string right =
          compileExpr(bin->right.get(), expectedType, forGlobalInit);
      return "pow(" + left + ", " + right + ")";
    }

    if (bin->op == BinaryOp::FLOOR_DIVIDE) {
      std::string left =
          compileExpr(bin->left.get(), expectedType, forGlobalInit);
      std::string right =
          compileExpr(bin->right.get(), expectedType, forGlobalInit);
      return "(double)floor((" + left + ") / (" + right + "))";
    }

    // detect lua-style ternary (condition and trueValue) or falseValue
    if (bin->op == BinaryOp::OR) {
      if (auto *leftBin = dynamic_cast<const BinaryExpr *>(bin->left.get())) {
        if (leftBin->op == BinaryOp::AND) {
          std::string condition = compileExpr(leftBin->left.get(), expectedType, forGlobalInit);
          std::string trueValue = compileExpr(leftBin->right.get(), expectedType, forGlobalInit);
          std::string falseValue = compileExpr(bin->right.get(), expectedType, forGlobalInit);
          return "(" + condition + ") ? " + trueValue + " : " + falseValue;
        }
      }
      
      // handle simple nil-coalescing: value or default
      ValueType leftType = inferExprType(bin->left.get());
      std::string left = compileExpr(bin->left.get(), expectedType, forGlobalInit);
      std::string right = compileExpr(bin->right.get(), expectedType, forGlobalInit);
      
      if (leftType == ValueType::STRING) {
        return "(!hl_is_nil_string(" + left + ") ? (" + left + ") : (" + right + "))";
      } else if (leftType == ValueType::NUMBER) {
        return "(!hl_is_nil_number(" + left + ") ? (" + left + ") : (" + right + "))";
      } else if (leftType == ValueType::BOOL) {
        return "(!hl_is_nil_bool(" + left + ") ? (" + left + ") : (" + right + "))";
      } else if (leftType == ValueType::ENUM) {
        return "((" + left + ") != -1 ? (" + left + ") : (" + right + "))";
      } else if (leftType == ValueType::STRUCT) {
        return "(!hl_is_nil_number(" + left + ") ? (" + left + ") : (" + right + "))";
      }
    }

    std::string left =
        compileExpr(bin->left.get(), expectedType, forGlobalInit);
    std::string right =
        compileExpr(bin->right.get(), expectedType, forGlobalInit);
    std::string op;

    switch (bin->op) {
    case BinaryOp::ADD:
      op = " + ";
      break;
    case BinaryOp::SUBTRACT:
      op = " - ";
      break;
    case BinaryOp::MULTIPLY:
      op = " * ";
      break;
    case BinaryOp::DIVIDE:
      op = " / ";
      break;
    case BinaryOp::MODULO:
      op = " % ";
      break;
    case BinaryOp::EQUAL:
      op = " == ";
      break;
    case BinaryOp::NOT_EQUAL:
      op = " != ";
      break;
    case BinaryOp::LESS:
      op = " < ";
      break;
    case BinaryOp::LESS_EQUAL:
      op = " <= ";
      break;
    case BinaryOp::GREATER:
      op = " > ";
      break;
    case BinaryOp::GREATER_EQUAL:
      op = " >= ";
      break;
    case BinaryOp::AND:
      op = " && ";
      break;
    case BinaryOp::OR:
      op = " || ";
      break;
    default:
      op = " + ";
      break;
    }
    return "(" + left + op + right + ")";
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    std::string operand =
        compileExpr(un->operand.get(), expectedType, forGlobalInit);
    if (un->op == UnaryOp::NEGATE) {
      return "(-" + operand + ")";
    } else if (un->op == UnaryOp::NOT) {
      ValueType operandType = inferExprType(un->operand.get());
      
      if (operandType == ValueType::STRUCT) {
        // check if it's an optional struct variable
        if (auto *varExpr = dynamic_cast<const VarExpr *>(un->operand.get())) {
          if (symbolTable.count(varExpr->name) && 
              symbolTable[varExpr->name].isOptional) {
            return "(isnan(" + operand + "))";
          }
        } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(un->operand.get())) {
          return "(isnan(" + operand + "))";
        }
      }
      
      return "(!" + operand + ")";
    }
  } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
    if (forGlobalInit) {
      return compileStructInitializer(structCons);
    } else {
      return compileStructConstructor(structCons);
    }
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
    if (auto *varExpr = dynamic_cast<const VarExpr *>(fieldAccess->object.get())) {
      std::string className = varExpr->name;
      
      if (classTable.count(className) > 0 && !symbolTable.count(className)) {
        const auto &classInfo = classTable[className];
        bool isStaticField = false;
        
        for (const auto &field : classInfo.fields) {
          if (field.name == fieldAccess->fieldName && field.isStatic) {
            isStaticField = true;
            break;
          }
        }
        
        if (isStaticField) {
          return className + "_" + fieldAccess->fieldName;
        }
      }
    }
    
    return compileFieldAccess(fieldAccess);
  } else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(expr)) {
    return compileClassInstantiation(classInst);
  } else if (auto *selfExpr = dynamic_cast<const SelfExpr *>(expr)) {
    return compileSelfExpr(selfExpr);
  }
  return "0.0";
}

std::string Compiler::compileExprForConcat(const Expr *expr) {
  ValueType type = inferExprType(expr);
  
  if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    if (call->name == "tostring") {
      return compileFunctionCall(call);
    }
  }
  
  // check if this is already a ternary expression that produces a string
  if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (bin->op == BinaryOp::OR) {
      if (auto *leftBin = dynamic_cast<const BinaryExpr *>(bin->left.get())) {
        if (leftBin->op == BinaryOp::AND) {
          // check if both branches are string literals
          ValueType trueType = inferExprType(leftBin->right.get());
          ValueType falseType = inferExprType(bin->right.get());
          
          if (trueType == ValueType::STRING && falseType == ValueType::STRING) {
            // both are strings, the ternary will produce a string directly
            return compileExpr(expr);
          }
        }
      }
    }
  }
  
  std::string compiled = compileExpr(expr);

  if (type == ValueType::NUMBER) {
    return "hl_tostring_number(" + compiled + ")";
  } else if (type == ValueType::BOOL) {
    return "hl_tostring_bool(" + compiled + ")";
  } else if (type == ValueType::STRING) {
    return compiled;
  } else if (type == ValueType::ENUM) {
    return "hl_tostring_number((double)" + compiled + ")";
  } else {
    return "hl_tostring_string(" + compiled + ")";
  }
}

bool Compiler::containsVariables(const Expr *expr) {
  if (dynamic_cast<const VarExpr *>(expr)) {
    return true;
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    for (const auto &arg : call->arguments) {
      if (containsVariables(arg.get())) return true;
    }
    return false;
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    return containsVariables(bin->left.get()) || containsVariables(bin->right.get());
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    return containsVariables(un->operand.get());
  } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
    for (const auto &arg : structCons->positionalArgs) {
      if (containsVariables(arg.get())) return true;
    }
    for (const auto &namedArg : structCons->namedArgs) {
      if (containsVariables(namedArg.second.get())) return true;
    }
    return false;
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
    return containsVariables(fieldAccess->object.get());
  } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    return containsVariables(unwrap->operand.get());
  }
  return false;
}

} // namespace HolyLua