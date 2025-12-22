#include "../../../include/compiler/compiler.h"
#include <set>

namespace HolyLua {

std::string Compiler::compileFieldAccess(const FieldAccessExpr *expr) {
  if (auto *varExpr = dynamic_cast<const VarExpr *>(expr->object.get())) {
    std::string className = varExpr->name;
    
    if (classTable.count(className) > 0 && !symbolTable.count(className)) {
      const auto &classInfo = classTable[className];
      bool isStaticField = false;
      
      for (const auto &field : classInfo.fields) {
        if (field.name == expr->fieldName && field.isStatic) {
          isStaticField = true;
          break;
        }
      }
      
      if (isStaticField) {
        return className + "_" + expr->fieldName;
      } else {
        error("Field '" + expr->fieldName + "' is not static in class '" + className + "'", expr->line);
        return "0";
      }
    }
  }
  
  std::string object = compileExpr(expr->object.get());
  
  bool isSelfPointer = false;
  if (auto *selfExpr = dynamic_cast<const SelfExpr *>(expr->object.get())) {
    if (currentFunction.find("___init") == std::string::npos && 
        !currentClass.empty()) {
      isSelfPointer = true;
    }
  }
  
  std::string result;
  if (isSelfPointer) {
    result = object + "->" + expr->fieldName;
  } else {
    result = object + "." + expr->fieldName;
  }

  return result;
}

void Compiler::compileFieldAssignment(const FieldAssignment *assign) {
  if (!assign)
    return;
  
  std::string objectExpr = compileExpr(assign->object.get());
  std::string fieldName = assign->fieldName;
  
  if (auto *varExpr = dynamic_cast<const VarExpr *>(assign->object.get())) {
    std::string className = varExpr->name;
    
    if (classTable.count(className) > 0 && !symbolTable.count(className)) {
      const auto &classInfo = classTable[className];
      bool isStaticField = false;
      bool isConstField = false;
      
      for (const auto &field : classInfo.fields) {
        if (field.name == fieldName && field.isStatic) {
          isStaticField = true;
          isConstField = field.isConst;
          break;
        }
      }
      
      if (isStaticField) {
        if (isConstField) {
          error("Cannot assign to const field '" + fieldName + "'", assign->line);
          return;
        }
        
        std::string valueExpr = compileExpr(assign->value.get());
        output += indent() + className + "_" + fieldName + " = " + valueExpr + ";\n";
        return;
      } else {
        error("Field '" + fieldName + "' is not static in class '" + className + "'", assign->line);
        return;
      }
    }
  }
  
  std::string typeName = "";
  
  if (auto *varExpr = dynamic_cast<const VarExpr *>(assign->object.get())) {
    if (symbolTable.count(varExpr->name)) {
      typeName = symbolTable[varExpr->name].structTypeName;
    }
  } else if (dynamic_cast<const SelfExpr *>(assign->object.get())) {
    typeName = currentClass;
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(assign->object.get())) {
    
  }
  
  if (!typeName.empty() && classTable.count(typeName)) {
    const auto &classInfo = classTable[typeName];
    
    for (const auto &field : classInfo.fields) {
      if (field.name == fieldName && !field.isStatic) {
        if (field.isConst) {
          if (currentFunction.find("___init") == std::string::npos) {
            error("Cannot assign to const field '" + fieldName + "' outside of constructor", assign->line);
            return;
          }
          
          static std::map<std::string, std::set<std::string>> constFieldAssignments;
          std::string key = currentClass + "_" + currentFunction;
          
          if (constFieldAssignments[key].count(fieldName) > 0) {
            error("Const field '" + fieldName + "' can only be assigned once in constructor", assign->line);
            return;
          }
          
          constFieldAssignments[key].insert(fieldName);
        }
        break;
      }
    }
  }
  
  std::string accessor = ".";
  
  if (auto *selfExpr = dynamic_cast<const SelfExpr *>(assign->object.get())) {
    if (currentFunction.find("___init") == std::string::npos && 
        !currentClass.empty()) {
      accessor = "->";
    }
  }
  
  output += indent() + objectExpr + accessor + fieldName;
  
  if (assign->isCompound) {
    std::string op;
    switch (assign->compoundOp) {
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
    case BinaryOp::POWER:
      output = indent() + objectExpr + accessor + fieldName + " = pow(" + 
               objectExpr + accessor + fieldName + ", " + 
               compileExpr(assign->value.get()) + ");\n";
      return;
    case BinaryOp::FLOOR_DIVIDE:
      output = indent() + objectExpr + accessor + fieldName + " = (double)floor(" +
               objectExpr + accessor + fieldName + " / " + 
               compileExpr(assign->value.get()) + ");\n";
      return;
    default:
      op = " + ";
      break;
    }
    std::string valueExpr = compileExpr(assign->value.get());
    output += " = " + objectExpr + accessor + fieldName + op + valueExpr;
  } else {
    std::string valueExpr = compileExpr(assign->value.get());
    output += " = " + valueExpr;
  }
  
  output += ";\n";
}

} // namespace HolyLua