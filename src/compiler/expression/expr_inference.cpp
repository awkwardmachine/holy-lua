#include "../../../include/compiler/compiler.h"

namespace HolyLua {

ValueType Compiler::inferExprType(const Expr *expr) {
  if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
    return inferType(lit->value);
  } else if (dynamic_cast<const NilExpr *>(expr)) {
    return ValueType::INFERRED;
  } else if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (symbolTable.count(var->name)) {
      return symbolTable[var->name].type;
    }
    return ValueType::INFERRED;
  } else if (auto *enumAccess = dynamic_cast<const EnumAccessExpr *>(expr)) {
    return ValueType::ENUM;
  } else if (dynamic_cast<const LambdaExpr *>(expr)) {
    return ValueType::FUNCTION;
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    if (call->name == "tostring") {
      return ValueType::STRING;
    }
    if (functionTable.count(call->name)) {
      return functionTable[call->name].returnType;
    }
    return ValueType::NUMBER;
  } else if (auto *methodCall = dynamic_cast<const MethodCall *>(expr)) {
    std::string className = "";
    if (auto *varExpr = dynamic_cast<const VarExpr *>(methodCall->object.get())) {
      if (symbolTable.count(varExpr->name)) {
        className = symbolTable[varExpr->name].structTypeName;
      } else if (classTable.count(varExpr->name)) {
        className = varExpr->name;
      }
    } else if (dynamic_cast<const SelfExpr *>(methodCall->object.get())) {
      className = currentClass;
    }
    
    if (!className.empty() && classTable.count(className)) {
      auto &classInfo = classTable[className];
      if (classInfo.methodInfo.count(methodCall->methodName)) {
        auto methodInfo = classInfo.methodInfo[methodCall->methodName];
        ValueType returnType = methodInfo.first;
        
        return returnType;
      }
    }
    return ValueType::INFERRED;
  } else if (dynamic_cast<const ClassInstantiation *>(expr)) {
    return ValueType::STRUCT;
  } else if (dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    return ValueType::NUMBER;
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (bin->op == BinaryOp::CONCAT) {
      return ValueType::STRING;
    }
    if (bin->op == BinaryOp::EQUAL || bin->op == BinaryOp::NOT_EQUAL ||
        bin->op == BinaryOp::LESS || bin->op == BinaryOp::LESS_EQUAL ||
        bin->op == BinaryOp::GREATER || bin->op == BinaryOp::GREATER_EQUAL) {
      return ValueType::BOOL;
    }
    if (bin->op == BinaryOp::AND || bin->op == BinaryOp::OR) {
      return ValueType::BOOL;
    }
    if (bin->op == BinaryOp::NIL_COALESCE) {
      return inferExprType(bin->right.get());
    }
    return ValueType::NUMBER;
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    if (un->op == UnaryOp::NOT) {
      return ValueType::BOOL;
    }
    return ValueType::NUMBER;
  } else if (dynamic_cast<const StructConstructor *>(expr)) {
    return ValueType::STRUCT;
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
    return inferFieldAccessType(fieldAccess);
  }
  return ValueType::INFERRED;
}

ValueType Compiler::inferFieldAccessType(const FieldAccessExpr *fieldAccess) {
  std::string structOrClassName = "";
  ValueType objectType = ValueType::INFERRED;
  
  // first, determine the type of the object being accessed
  if (auto *varExpr = dynamic_cast<const VarExpr *>(fieldAccess->object.get())) {
    if (symbolTable.count(varExpr->name)) {
      structOrClassName = symbolTable[varExpr->name].structTypeName;
      objectType = symbolTable[varExpr->name].type;
    }
  } else if (dynamic_cast<const SelfExpr *>(fieldAccess->object.get())) {
    structOrClassName = currentClass;
    objectType = ValueType::STRUCT;
  } else if (auto *nestedFieldAccess = dynamic_cast<const FieldAccessExpr *>(fieldAccess->object.get())) {
    // handle nested field access recursively
    objectType = inferFieldAccessType(nestedFieldAccess);
    
    // if the nested field access returns a struct, get its type name
    if (objectType == ValueType::STRUCT) {
      structOrClassName = getStructTypeNameFromFieldAccess(nestedFieldAccess);
    }
  }
  
  // now look up the field in the appropriate table
  if (!structOrClassName.empty()) {
    if (classTable.count(structOrClassName)) {
      auto &classInfo = classTable[structOrClassName];
      if (classInfo.fieldInfo.count(fieldAccess->fieldName)) {
        return classInfo.fieldInfo[fieldAccess->fieldName].first;
      }
    }
    
    if (structTable.count(structOrClassName)) {
      const auto &structInfo = structTable[structOrClassName];
      for (const auto &field : structInfo.fields) {
        if (field.name == fieldAccess->fieldName) {
          return field.type;
        }
      }
    }
  }
  
  return ValueType::INFERRED;
}

std::string Compiler::getStructTypeNameFromFieldAccess(const FieldAccessExpr *fieldAccess) {
  std::string structOrClassName = "";
  
  // determine the object's struct type
  if (auto *varExpr = dynamic_cast<const VarExpr *>(fieldAccess->object.get())) {
    if (symbolTable.count(varExpr->name)) {
      structOrClassName = symbolTable[varExpr->name].structTypeName;
    }
  } else if (dynamic_cast<const SelfExpr *>(fieldAccess->object.get())) {
    structOrClassName = currentClass;
  } else if (auto *nestedFieldAccess = dynamic_cast<const FieldAccessExpr *>(fieldAccess->object.get())) {
    // recursively resolve nested accesses
    structOrClassName = getStructTypeNameFromFieldAccess(nestedFieldAccess);
  }
  
  if (structOrClassName.empty()) {
    return "";
  }
  
  if (classTable.count(structOrClassName)) {
    auto &classInfo = classTable[structOrClassName];
    if (classInfo.fieldInfo.count(fieldAccess->fieldName)) {
      auto fieldType = classInfo.fieldInfo[fieldAccess->fieldName].first;
      if (fieldType == ValueType::STRUCT) {
        // find the struct type name for this field
        for (const auto &field : classInfo.fields) {
          if (field.name == fieldAccess->fieldName) {
            return field.structTypeName;
          }
        }
      }
    }
  }
  
  // check struct table
  if (structTable.count(structOrClassName)) {
    const auto &structInfo = structTable[structOrClassName];
    for (const auto &field : structInfo.fields) {
      if (field.name == fieldAccess->fieldName) {
        if (field.type == ValueType::STRUCT) {
          return field.structTypeName;
        }
      }
    }
  }
  
  return "";
}

} // namespace HolyLua