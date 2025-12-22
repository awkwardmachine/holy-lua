#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compileConstructor(const std::string &className, 
                                   const ClassMethod &constructor) {
  std::string savedFunction = currentFunction;
  std::string savedClass = currentClass;
  currentFunction = className + "___init";
  currentClass = className;
  
  auto savedSymbolTable = symbolTable;
  
  output += className + " " + className + "_new(";
  
  const auto &classInfo = classTable[className];
  
  for (size_t i = 0; i < constructor.parameters.size(); i++) {
    const auto &param = constructor.parameters[i];
    
    std::string paramType;
    
    // look up the field type from the class definition
    std::string fieldTypeName = "";
    for (const auto &field : classInfo.fields) {
      if (field.name == param.first) {
        if (field.type == ValueType::STRUCT) {
          fieldTypeName = field.structTypeName;
        } else if (field.type == ValueType::ENUM) {
          fieldTypeName = field.structTypeName;
        }
        break;
      }
    }
    
    if (param.second == ValueType::STRUCT) {
      if (!fieldTypeName.empty()) {
        paramType = fieldTypeName;
      } else if (i < constructor.parameterTypeNames.size() && !constructor.parameterTypeNames[i].empty()) {
        paramType = constructor.parameterTypeNames[i];
      } else {
        paramType = "void*";
      }
    } else if (param.second == ValueType::ENUM) {
      if (!fieldTypeName.empty()) {
        paramType = fieldTypeName;
      } else if (i < constructor.parameterTypeNames.size() && !constructor.parameterTypeNames[i].empty()) {
        paramType = constructor.parameterTypeNames[i];
      } else {
        paramType = "int";
      }
    } else {
      paramType = getCType(param.second);
    }
    
    output += paramType + " " + param.first;
    
    if (i < constructor.parameters.size() - 1) {
      output += ", ";
    }
  }
  
  output += ") {\n";
  indentLevel = 1;
  
  output += indent() + className + " self = {0};\n";

  Variable selfVar;
  selfVar.type = ValueType::STRUCT;
  selfVar.isConst = false;
  selfVar.isDefined = true;
  selfVar.isOptional = false;
  selfVar.isFunction = false;
  selfVar.isStruct = true;
  selfVar.structTypeName = className;
  symbolTable["self"] = selfVar;
  
  // add all parameters to symbol table
  for (size_t i = 0; i < constructor.parameters.size(); i++) {
    const auto &param = constructor.parameters[i];
    bool isOptional = (i < constructor.parameterOptionals.size()) && constructor.parameterOptionals[i];
    
    std::string structTypeName = "";
    bool isStruct = false;
    
    for (const auto &field : classInfo.fields) {
      if (field.name == param.first) {
        if (field.type == ValueType::STRUCT || field.type == ValueType::ENUM) {
          structTypeName = field.structTypeName;
          isStruct = (field.type == ValueType::STRUCT);
        }
        break;
      }
    }
    
    if (structTypeName.empty() && param.second == ValueType::STRUCT) {
      isStruct = true;
      if (i < constructor.parameterTypeNames.size() && !constructor.parameterTypeNames[i].empty()) {
        structTypeName = constructor.parameterTypeNames[i];
      }
    } else if (structTypeName.empty() && param.second == ValueType::ENUM) {
      if (i < constructor.parameterTypeNames.size() && !constructor.parameterTypeNames[i].empty()) {
        structTypeName = constructor.parameterTypeNames[i];
      }
    }

    Variable paramVar;
    paramVar.type = param.second;
    paramVar.isConst = false;
    paramVar.isDefined = true;
    paramVar.isOptional = isOptional;
    paramVar.isFunction = false;
    paramVar.isStruct = isStruct;
    paramVar.structTypeName = structTypeName;
    symbolTable[param.first] = paramVar;
  }
  
  // compile constructor body
  for (const auto &stmt : constructor.body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }
  
  output += indent() + "return self;\n";
  
  indentLevel = 0;
  output += "}\n\n";
  
  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
  currentClass = savedClass;
}

void Compiler::compileMethod(const std::string &className, const ClassMethod &method) {
  std::string savedFunction = currentFunction;
  std::string savedClass = currentClass;
  
  if (method.isStatic && method.name == "new") {
    currentFunction = className + "_static_new";
  } else {
    currentFunction = className + "_" + method.name;
  }
  
  currentClass = className;
  
  auto savedSymbolTable = symbolTable;
  
  ValueType actualReturnType = method.returnType;
  
  // infer return type
  if (actualReturnType == ValueType::INFERRED) {
    bool hasReturnValue = false;
    
    for (const auto &stmt : method.body) {
      if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
        if (ret->value) {
          hasReturnValue = true;
          actualReturnType = inferExprType(ret->value.get());
          break;
        }
      }
    }
    
    if (!hasReturnValue && method.isStatic && 
        (method.name == "new" || method.name.find("create") != std::string::npos ||
         method.name.find("New") != std::string::npos)) {
      actualReturnType = ValueType::STRUCT;
    }
  }
  
  std::string returnType;
  if (actualReturnType == ValueType::STRUCT) {
    returnType = className;
  } else if (actualReturnType == ValueType::INFERRED) {
    // for inferred type with no return value, use void
    bool hasReturnValue = false;
    for (const auto &stmt : method.body) {
      if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
        if (ret->value) {
          hasReturnValue = true;
          break;
        }
      }
    }
    returnType = hasReturnValue ? "double" : "void";
  } else {
    returnType = getCType(actualReturnType);
  }
  
  std::string methodName;
  if (method.isStatic && method.name == "new") {
    methodName = className + "_static_new";
  } else {
    methodName = className + "_" + method.name;
  }
  
  output += returnType + " " + methodName + "(";
  
  if (!method.isStatic) {
    output += className + "* self";
    if (!method.parameters.empty()) {
      output += ", ";
    }
  }
  
  for (size_t i = 0; i < method.parameters.size(); i++) {
    const auto &param = method.parameters[i];
    std::string paramType;
    
    if (param.second == ValueType::STRUCT) {
      if (i < method.parameterTypeNames.size() && !method.parameterTypeNames[i].empty()) {
        paramType = method.parameterTypeNames[i];
      } else {
        paramType = "void*";
      }
    } else if (param.second == ValueType::ENUM) {
      if (i < method.parameterTypeNames.size() && !method.parameterTypeNames[i].empty()) {
        paramType = method.parameterTypeNames[i];
      } else {
        paramType = "int";
      }
    } else {
      paramType = getCType(param.second);
    }
    
    output += paramType + " " + param.first;
    
    if (i < method.parameters.size() - 1) {
      output += ", ";
    }
  }
  
  output += ") {\n";
  indentLevel = 1;
  
  if (!method.isStatic) {
    Variable selfVar;
    selfVar.type = ValueType::STRUCT;
    selfVar.isConst = false;
    selfVar.isDefined = true;
    selfVar.isOptional = false;
    selfVar.isFunction = false;
    selfVar.isStruct = true;
    selfVar.structTypeName = className;
    symbolTable["self"] = selfVar;
  }
  
  for (size_t i = 0; i < method.parameters.size(); i++) {
    const auto &param = method.parameters[i];
    bool isOptional = method.parameterOptionals[i];
    
    std::string structTypeName = "";
    bool isStruct = false;
    if (param.second == ValueType::STRUCT) {
      isStruct = true;
      if (i < method.parameterTypeNames.size() && !method.parameterTypeNames[i].empty()) {
        structTypeName = method.parameterTypeNames[i];
      }
    } else if (param.second == ValueType::ENUM) {
      if (i < method.parameterTypeNames.size() && !method.parameterTypeNames[i].empty()) {
        structTypeName = method.parameterTypeNames[i];
      }
    }
    
    Variable paramVar;
    paramVar.type = param.second;
    paramVar.isConst = false;
    paramVar.isDefined = true;
    paramVar.isOptional = isOptional;
    paramVar.isFunction = false;
    paramVar.isStruct = isStruct;
    paramVar.structTypeName = structTypeName;
    symbolTable[param.first] = paramVar;
  }
  
  for (const auto &stmt : method.body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }
  
  bool hasReturnAtEnd = false;
  if (!method.body.empty()) {
    hasReturnAtEnd = dynamic_cast<const ReturnStmt *>(method.body.back().get()) != nullptr;
  }
  
  // don't add default return for void methods
  if (!hasReturnAtEnd && returnType != "void") {
    if (actualReturnType == ValueType::NUMBER || actualReturnType == ValueType::INFERRED) {
      output += indent() + "return 0.0;\n";
    } else if (actualReturnType == ValueType::STRING) {
      output += indent() + "return \"\";\n";
    } else if (actualReturnType == ValueType::BOOL) {
      output += indent() + "return 0;\n";
    } else if (actualReturnType == ValueType::STRUCT) {
      output += indent() + className + " result = {0};\n";
      output += indent() + "return result;\n";
    }
  }
  
  indentLevel = 0;
  output += "}\n\n";
  
  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
  currentClass = savedClass;
}

std::string Compiler::compileMethodCall(const MethodCall *call) {
  std::string objectExpr = compileExpr(call->object.get());
  
  bool isStatic = false;
  std::string className = "";
  
  if (auto *varExpr = dynamic_cast<const VarExpr *>(call->object.get())) {
    if (classTable.count(varExpr->name)) {
      isStatic = true;
      className = varExpr->name;
    } else if (symbolTable.count(varExpr->name) && symbolTable[varExpr->name].isDefined) {
      className = symbolTable[varExpr->name].structTypeName;
    } else if (symbolTable.count(varExpr->name) && !symbolTable[varExpr->name].isDefined) {
      error("Variable '" + varExpr->name + "' is declared but not initialized", call->line);
      return "0";
    }
  } else if (dynamic_cast<const SelfExpr *>(call->object.get())) {
    className = currentClass;
  }
  
  if (className.empty()) {
    error("Cannot determine class type for method call '" + call->methodName + "'", call->line);
    return "0";
  }
  
  if (!classTable.count(className)) {
    error("Class '" + className + "' not defined", call->line);
    return "0";
  }
  
  auto &classInfo = classTable[className];
  
  if (!classInfo.methodInfo.count(call->methodName)) {
    error("Method '" + call->methodName + "' does not exist in class '" + className + "'", call->line);
    return "0";
  }
  
  std::string result;
  
  if (isStatic) {
    std::string actualMethodName;
    if (call->methodName == "new") {
      actualMethodName = className + "_static_new";
    } else {
      actualMethodName = className + "_" + call->methodName;
    }
    
    result = actualMethodName + "(";
    
    for (size_t i = 0; i < call->arguments.size(); i++) {
      result += compileExpr(call->arguments[i].get());
      if (i < call->arguments.size() - 1) {
        result += ", ";
      }
    }
  } else {
    std::string methodName = className + "_" + call->methodName;
    result = methodName + "(";
    
    bool isSelfPointer = false;
    if (dynamic_cast<const SelfExpr *>(call->object.get())) {
      if (currentFunction.find("___init") == std::string::npos && 
          !currentClass.empty()) {
        isSelfPointer = true;
      }
    }
    
    if (isSelfPointer) {
      result += objectExpr;
    } else {
      result += "&(" + objectExpr + ")";
    }
    
    if (!call->arguments.empty()) {
      result += ", ";
    }
    
    for (size_t i = 0; i < call->arguments.size(); i++) {
      result += compileExpr(call->arguments[i].get());
      if (i < call->arguments.size() - 1) {
        result += ", ";
      }
    }
  }
  
  result += ")";
  return result;
}

} // namespace HolyLua