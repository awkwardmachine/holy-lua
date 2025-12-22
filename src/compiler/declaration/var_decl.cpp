#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compileVarDecl(const VarDecl *decl) {
  if (decl->isGlobal)
    return;

  ValueType actualType = decl->type;
  std::string structTypeName = decl->typeName;

  if (actualType == ValueType::STRUCT && structTypeName == "struct" && decl->hasValue) {
    if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
      structTypeName = classInst->className;
    } else if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
      structTypeName = structCons->structName;
    } else if (auto *methodCall = dynamic_cast<const MethodCall *>(decl->value.get())) {
      std::string className = "";
      if (auto *varExpr = dynamic_cast<const VarExpr *>(methodCall->object.get())) {
        if (classTable.count(varExpr->name)) {
          className = varExpr->name;
        }
      }
      
      if (!className.empty()) {
        structTypeName = className;
      }
    }
  }
  
  if (actualType == ValueType::INFERRED && decl->hasValue) {
    actualType = inferExprType(decl->value.get());
    
    if (actualType == ValueType::STRUCT && decl->value) {
      if (auto *methodCall = dynamic_cast<const MethodCall *>(decl->value.get())) {
        std::string className = "";
        if (auto *varExpr = dynamic_cast<const VarExpr *>(methodCall->object.get())) {
          if (classTable.count(varExpr->name)) {
            className = varExpr->name;
          }
        }
        
        if (!className.empty()) {
          structTypeName = className;
        }
      } else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
        structTypeName = classInst->className;
      }
    }
  }

  // handle lambda expressions
  if (decl->hasValue) {
    if (auto *lambda = dynamic_cast<const LambdaExpr *>(decl->value.get())) {
      std::string funcName = decl->name;
      std::string savedOutput = output;
      output = "";

      compileLambdaExpr(lambda, funcName);

      nestedFunctionDecls += output;
      output = savedOutput;

      Variable var;
      var.type = actualType;
      var.isConst = decl->isConst;
      var.isDefined = true;
      var.isOptional = decl->isOptional;
      var.isFunction = true;
      var.isStruct = false;
      var.structTypeName = "";
      symbolTable[decl->name] = var;
      return;
    }
    // handle class instantiation
    else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
      std::string className = classInst->className;

      output += indent();
      if (decl->isConst)
        output += "const ";
      output += className + " " + decl->name;
      output += " = " + compileExpr(decl->value.get(), ValueType::STRUCT, false) + ";\n";

      Variable var;
      var.type = ValueType::STRUCT;
      var.isConst = decl->isConst;
      var.isDefined = true;
      var.isOptional = decl->isOptional;
      var.isFunction = false;
      var.isStruct = true;
      var.structTypeName = className;
      symbolTable[decl->name] = var;
      return;
    }
    // handle struct constructor
    else if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
      std::string structName = structCons->structName;

      output += indent();
      if (decl->isConst)
        output += "const ";
      output += structName + " " + decl->name;
      output += " = " + compileExpr(decl->value.get(), ValueType::STRUCT, false) + ";\n";

      Variable var;
      var.type = ValueType::STRUCT;
      var.isConst = decl->isConst;
      var.isDefined = true;
      var.isOptional = decl->isOptional;
      var.isFunction = false;
      var.isStruct = true;
      var.structTypeName = structName;
      symbolTable[decl->name] = var;
      return;
    }
    // handle enum access expressions, infer the enum type name
    else if (auto *enumAccess = dynamic_cast<const EnumAccessExpr *>(decl->value.get())) {
      structTypeName = enumAccess->enumName;
    }
  }

  std::string ctype;
  
  if (actualType == ValueType::STRUCT && structTypeName == "struct") {
    error("Cannot determine struct type for variable '" + decl->name + "'. "
          "Please specify the exact type or provide a value from which the type can be inferred.", 
          decl->line);
    return;
  }
  
  if (actualType == ValueType::STRUCT && !structTypeName.empty()) {
    ctype = structTypeName;
  } else if (actualType == ValueType::ENUM && !structTypeName.empty()) {
    ctype = structTypeName;
  } else if (actualType == ValueType::ENUM && !decl->typeName.empty()) {
    ctype = decl->typeName;
    structTypeName = decl->typeName;
  } else {
    ctype = getCType(actualType);
  }

  // generate declaration
  output += indent();
  if (decl->isConst)
    output += "const ";
  output += ctype + " " + decl->name;

  if (decl->hasValue) {
    output += " = " + compileExpr(decl->value.get(), actualType, false);
  } else if (decl->isOptional) {
    // initialize optional variables to nil
    if (actualType == ValueType::ENUM) {
      output += " = -1";
    } else if (actualType == ValueType::STRING) {
      output += " = NULL";
    } else if (actualType == ValueType::NUMBER) {
      output += " = HL_NIL_NUMBER";
    } else if (actualType == ValueType::BOOL) {
      output += " = -1";
    }
  }

  output += ";\n";

  // register in symbol table
  Variable var;
  var.type = actualType;
  var.isConst = decl->isConst;
  var.isDefined = true;
  var.isOptional = decl->isOptional;
  var.isFunction = false;
  var.isStruct = (actualType == ValueType::STRUCT);
  var.structTypeName = structTypeName;
  symbolTable[decl->name] = var;
}

void Compiler::compileGlobalVarDecl(const VarDecl *decl, std::string &globalDecls,
                                    std::vector<std::pair<std::string, std::string>> &deferredInitializations) {
  ValueType actualType = decl->type;
  std::string structTypeName = decl->typeName;

  if (actualType == ValueType::INFERRED && decl->hasValue) {
    actualType = inferExprType(decl->value.get());
    
    // infer enum type name from enum access
    if (actualType == ValueType::ENUM && decl->hasValue) {
      if (auto *enumAccess = dynamic_cast<const EnumAccessExpr *>(decl->value.get())) {
        structTypeName = enumAccess->enumName;
      }
    }
  }

  // handle lambda expressions
  if (decl->hasValue) {
    if (auto *lambda = dynamic_cast<const LambdaExpr *>(decl->value.get())) {
      std::string funcName = decl->name;
      std::string savedOutput = output;
      output = "";

      compileLambdaExpr(lambda, funcName);
      nestedFunctionDecls += output;
      output = savedOutput;

      Variable var;
      var.type = actualType;
      var.isConst = decl->isConst;
      var.isDefined = true;
      var.isOptional = decl->isOptional;
      var.isFunction = true;
      var.isStruct = false;
      var.structTypeName = "";
      symbolTable[decl->name] = var;
      return;
    }
  }

  std::string ctype = getCType(actualType);

  if (actualType == ValueType::STRUCT && decl->hasValue) {
    if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
      ctype = structCons->structName;
      if (structTypeName.empty()) {
        structTypeName = structCons->structName;
      }
    } else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
      ctype = classInst->className;
      if (structTypeName.empty()) {
        structTypeName = classInst->className;
      }
    }
  }
  
  if (actualType == ValueType::STRUCT && !decl->typeName.empty()) {
    ctype = decl->typeName;
    if (structTypeName.empty()) {
      structTypeName = decl->typeName;
    }
  }
  
  if (actualType == ValueType::ENUM && !structTypeName.empty()) {
    ctype = structTypeName;
  } else if (actualType == ValueType::ENUM && !decl->typeName.empty()) {
    ctype = decl->typeName;
    structTypeName = decl->typeName;
  }

  // check if initialization can be done globally
  bool canBeGlobalInitializer = true;
  std::string initExpr = "";
  
  if (decl->hasValue) {
    if (actualType == ValueType::STRUCT) {
      if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
        for (const auto &arg : structCons->positionalArgs) {
          if (dynamic_cast<const VarExpr *>(arg.get())) {
            canBeGlobalInitializer = false;
            break;
          }
        }
        for (const auto &namedArg : structCons->namedArgs) {
          if (dynamic_cast<const VarExpr *>(namedArg.second.get())) {
            canBeGlobalInitializer = false;
            break;
          }
        }
        
        if (canBeGlobalInitializer) {
          initExpr = compileExpr(decl->value.get(), actualType, true);
        }
      } else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
        canBeGlobalInitializer = false;
      }
    } else if (actualType == ValueType::ENUM) {
      if (containsVariables(decl->value.get())) {
        canBeGlobalInitializer = false;
      } else {
        initExpr = compileExpr(decl->value.get(), actualType, true);
      }
    } else {
      if (containsVariables(decl->value.get())) {
        canBeGlobalInitializer = false;
      } else {
        initExpr = compileExpr(decl->value.get(), actualType, true);
      }
    }
  }

  // generate global declaration
  if (decl->isConst) globalDecls += "const ";
  globalDecls += ctype + " " + decl->name;
  
  if (canBeGlobalInitializer && !initExpr.empty()) {
    globalDecls += " = " + initExpr + ";\n";
  } else {
    // initialize with default value if optional
    if (!decl->hasValue && decl->isOptional) {
      if (actualType == ValueType::ENUM) {
        globalDecls += " = -1;\n";
      } else if (actualType == ValueType::STRING) {
        globalDecls += " = NULL;\n";
      } else if (actualType == ValueType::NUMBER) {
        globalDecls += " = HL_NIL_NUMBER;\n";
      } else if (actualType == ValueType::BOOL) {
        globalDecls += " = -1;\n";
      } else {
        globalDecls += ";\n";
      }
    } else {
      globalDecls += ";\n";
    }
    
    // defer initialization to main if value is provided
    if (decl->hasValue) {
      std::string initCode;
      if (actualType == ValueType::STRUCT) {
        if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
          initCode = decl->name + " = " + compileStructConstructor(structCons) + ";";
        } else if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
          initCode = decl->name + " = " + compileClassInstantiation(classInst) + ";";
        } else {
          initCode = decl->name + " = " + compileExpr(decl->value.get(), actualType, false) + ";";
        }
      } else {
        initCode = decl->name + " = " + compileExpr(decl->value.get(), actualType, false) + ";";
      }
      deferredInitializations.push_back({decl->name, initCode});
    }
  }

  // register in symbol table
  Variable var;
  var.type = actualType;
  var.isConst = decl->isConst;
  var.isDefined = true;
  var.isOptional = decl->isOptional;
  var.isFunction = false;
  var.isStruct = (actualType == ValueType::STRUCT);
  var.structTypeName = structTypeName;
  symbolTable[decl->name] = var;
}

void Compiler::compileAssignment(const Assignment *assign) {
  if (!checkVariable(assign->name)) {
    error("Variable '" + assign->name +
              "' not defined. Use 'local' or 'global' to declare it.",
          assign->line);
    output = "";
    return;
  }

  auto &var = symbolTable[assign->name];
  if (var.isConst) {
    error("Cannot assign to const variable '" + assign->name + "'",
          assign->line);
    output = "";
    return;
  }

  output += indent() + assign->name;

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
      output = indent() + assign->name + " = pow(" + assign->name + ", " +
               compileExpr(assign->value.get(), var.type) + ");\n";
      return;
    case BinaryOp::FLOOR_DIVIDE:
      output = indent() + assign->name + " = (double)floor(" + assign->name +
               " / " + compileExpr(assign->value.get(), var.type) + ");\n";
      return;
    default:
      op = " + ";
      break;
    }
    output +=
        " = " + assign->name + op + compileExpr(assign->value.get(), var.type);
  } else {
    output += " = " + compileExpr(assign->value.get(), var.type);
  }

  output += ";\n";
}

void Compiler::compileReturnStmt(const ReturnStmt *ret) {
  if (currentFunction.empty()) {
    error("Return statement outside of function", ret->line);
    output = "";
    return;
  }

  output += indent() + "return";

  if (ret->value) {
    ValueType returnType = ValueType::INFERRED;
    if (functionTable.count(currentFunction)) {
      returnType = functionTable[currentFunction].returnType;
    }

    output += " " + compileExpr(ret->value.get(), returnType);
  }

  output += ";\n";
}

} // namespace HolyLua