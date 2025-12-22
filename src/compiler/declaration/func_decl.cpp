#include "../../../include/compiler/compiler.h"

namespace HolyLua {

std::string Compiler::compileFunctionCall(const FunctionCall *call) {
  if (call->name == "tostring") {
    if (call->arguments.size() != 1) {
      error("tostring() expects exactly 1 argument", call->line);
      return "";
    }

    std::string arg = compileExpr(call->arguments[0].get());
    ValueType argType = inferExprType(call->arguments[0].get());

    if (argType == ValueType::NUMBER) {
      return "hl_tostring_number(" + arg + ")";
    } else if (argType == ValueType::STRING) {
      return "hl_tostring_string(" + arg + ")";
    } else if (argType == ValueType::BOOL) {
      return "hl_tostring_bool(" + arg + ")";
    } else {
      return "hl_tostring_number(" + arg + ")";
    }
  }

  if (call->name == "tonumber") {
    if (call->arguments.size() != 1) {
      error("tonumber() expects exactly 1 argument", call->line);
      return "";
    }

    std::string arg = compileExpr(call->arguments[0].get());
    ValueType argType = inferExprType(call->arguments[0].get());

    if (argType == ValueType::STRING || argType == ValueType::INFERRED) {
      return "hl_tonumber(" + arg + ")";
    } else {
      error("tonumber() expects a string argument", call->line);
      return "";
    }
  }

  if (call->name == "print") {
    error("print() should be used as a statement, not an expression",
          call->line);
    return "";
  }

  if (call->name == "type") {
    if (call->arguments.size() != 1) {
      error("type() expects exactly 1 argument", call->line);
      return "";
    }

    std::string arg = compileExpr(call->arguments[0].get());
    ValueType argType = inferExprType(call->arguments[0].get());

    switch (argType) {
    case ValueType::NUMBER:
      return "\"number\"";
    case ValueType::STRING:
      return "\"string\"";
    case ValueType::BOOL:
      return "\"bool\"";
    case ValueType::INFERRED:
      return "\"nil\"";
    default:
      return "\"unknown\"";
    }
  }

  if (!checkFunction(call->name)) {
    if (symbolTable.count(call->name) && symbolTable[call->name].isDefined) {
      error("Variable '" + call->name +
                "' exists but is not callable as a function",
            call->line);
      return "";
    }
    error("Function '" + call->name + "' is not declared", call->line);
    return "";
  }

  const auto &funcInfo = functionTable[call->name];

  bool isNestedFunction = !funcInfo.isGlobal;

  size_t paramCount = funcInfo.parameters.size();
  size_t argCount = call->arguments.size();

  size_t expectedArgs = isNestedFunction
                            ? (paramCount - currentFunctionParams.size())
                            : paramCount;

  size_t requiredParams = 0;

  if (isNestedFunction) {
    size_t ownParamStart = currentFunctionParams.size();
    for (size_t i = ownParamStart; i < paramCount; i++) {
      size_t optionalIdx = i - ownParamStart;
      if (optionalIdx < funcInfo.parameterOptionals.size() &&
          !funcInfo.parameterOptionals[optionalIdx]) {
        requiredParams++;
      }
    }
  } else {
    for (size_t i = 0; i < paramCount; i++) {
      if (i < funcInfo.parameterOptionals.size() &&
          !funcInfo.parameterOptionals[i]) {
        requiredParams++;
      }
    }
  }

  if (argCount < requiredParams) {
    error("Function '" + call->name + "' requires at least " +
              std::to_string(requiredParams) + " argument(s), but got " +
              std::to_string(argCount),
          call->line);
    return "";
  }

  if (argCount > expectedArgs) {
    error("Function '" + call->name + "' expects at most " +
              std::to_string(expectedArgs) + " argument(s), but got " +
              std::to_string(argCount),
          call->line);
    return "";
  }

  std::string result = call->name + "(";

  if (isNestedFunction) {
    for (size_t i = 0; i < currentFunctionParams.size(); i++) {
      const auto &param = currentFunctionParams[i];
      result += param.first;

      if (i < currentFunctionParams.size() - 1 || argCount > 0) {
        result += ", ";
      }
    }
  }

  // handle provided arguments, detecting explicit nil
  for (size_t i = 0; i < argCount; i++) {
    size_t paramIdx = isNestedFunction ? (currentFunctionParams.size() + i) : i;

    if (paramIdx < funcInfo.parameters.size()) {
      ValueType paramType = funcInfo.parameters[paramIdx].second;
      
      // check if this is an explicit nil for an optional parameter
      bool isNilArg = dynamic_cast<const NilExpr *>(call->arguments[i].get()) != nullptr;
      
      if (isNilArg && paramIdx < funcInfo.parameterOptionals.size() && 
          funcInfo.parameterOptionals[paramIdx]) {
        // this is an explicit nil for an optional parameter
        if (paramType == ValueType::STRING) {
          result += "(char*)0";
        } else if (paramType == ValueType::NUMBER) {
          result += "HL_NIL_NUMBER";
        } else if (paramType == ValueType::BOOL) {
          result += "-1";
        } else if (paramType == ValueType::ENUM) {
          result += "-1";
        } else {
          result += "HL_NIL_NUMBER";
        }
      } else {
        result += compileExpr(call->arguments[i].get(), paramType);
      }

      if (i < argCount - 1) {
        result += ", ";
      }
    }
  }

  size_t providedArgs =
      isNestedFunction ? (currentFunctionParams.size() + argCount) : argCount;

  // fill in nil values for omitted optional parameters
  for (size_t i = providedArgs; i < paramCount; i++) {
    if (i > 0 || (isNestedFunction && currentFunctionParams.size() > 0) ||
        argCount > 0) {
      result += ", ";
    }

    ValueType paramType = funcInfo.parameters[i].second;
    
    if (paramType == ValueType::NUMBER) {
      result += "HL_NIL_NUMBER";
    } else if (paramType == ValueType::STRING) {
      result += "(char*)0";
    } else if (paramType == ValueType::BOOL) {
      result += "-1";
    } else if (paramType == ValueType::ENUM) {
      result += "-1";
    } else {
      result += "HL_NIL_NUMBER";
    }
  }

  result += ")";
  return result;
}

void Compiler::compileFunctionDecl(const FunctionDecl *func) {
  std::string savedFunction = currentFunction;
  currentFunction = func->name;

  auto savedFunctionParams = currentFunctionParams;
  currentFunctionParams = func->parameters;

  auto savedSymbolTable = symbolTable;

  std::vector<std::pair<std::string, ValueType>> parentParams;
  if (!func->isGlobal) {
    // nested function
  } else {
    parentParams = func->parameters;
  }

  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }

    bool isOptional = (i < func->parameterOptionals.size()) ? func->parameterOptionals[i] : false;

    symbolTable[param.first] = {paramType, false, true, isOptional, false, false, ""};
  }

  for (const auto &stmt : func->body) {
    if (auto *nestedFunc = dynamic_cast<const FunctionDecl *>(stmt.get())) {
      FunctionInfo nestedInfo;
      nestedInfo.name = nestedFunc->name;

      std::vector<std::pair<std::string, ValueType>> allParams =
          func->parameters;
      allParams.insert(allParams.end(), nestedFunc->parameters.begin(),
                       nestedFunc->parameters.end());

      nestedInfo.parameters = allParams;
      nestedInfo.parameterOptionals = nestedFunc->parameterOptionals;
      nestedInfo.returnType = nestedFunc->returnType;
      nestedInfo.isGlobal = false;
      nestedInfo.nestedFunctions = {};
      functionTable[nestedFunc->name] = nestedInfo;

      symbolTable[nestedFunc->name] = {ValueType::INFERRED, false, true, false,
                                       false};

      std::string savedOutput = output;
      output = "";

      compileNestedFunction(nestedFunc, func->parameters);

      nestedFunctionDecls += output + "\n";
      output = savedOutput;
    }
  }

  ValueType actualReturnType = func->returnType;

  if (actualReturnType == ValueType::INFERRED) {
    for (const auto &stmt : func->body) {
      if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
        if (ret->value) {
          actualReturnType = inferExprType(ret->value.get());

          if (actualReturnType == ValueType::INFERRED) {
            if (auto *call =
                    dynamic_cast<const FunctionCall *>(ret->value.get())) {
              if (call->name == "tostring") {
                actualReturnType = ValueType::STRING;
              }
            } else if (auto *bin =
                           dynamic_cast<const BinaryExpr *>(ret->value.get())) {
              if (bin->op == BinaryOp::CONCAT) {
                actualReturnType = ValueType::STRING;
              }
            }
          }
          break;
        }
      }
    }

    if (actualReturnType == ValueType::INFERRED) {
      for (const auto &stmt : func->body) {
        if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
          if (ret->value) {
            if (isStringExpr(ret->value.get())) {
              actualReturnType = ValueType::STRING;
            } else if (isNumberExpr(ret->value.get())) {
              actualReturnType = ValueType::NUMBER;
            } else if (isBoolExpr(ret->value.get())) {
              actualReturnType = ValueType::BOOL;
            }
            break;
          }
        }
      }
    }
  }

  if (func->isGlobal) {
    functionTable[func->name].returnType = actualReturnType;
  }

  std::string returnType = getCType(actualReturnType);

  if (func->name == "main") {
    returnType = "int";
  }

  output += returnType + " " + func->name + "(";

  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }

    std::string paramCType = getCType(paramType);
    output += paramCType + " " + param.first;
    if (i < func->parameters.size() - 1) {
      output += ", ";
    }
  }

  output += ") {\n";

  int savedIndent = indentLevel;
  indentLevel = 1;

  for (const auto &stmt : func->body) {
    if (dynamic_cast<const FunctionDecl *>(stmt.get())) {
      continue;
    }

    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  bool hasReturnAtEnd = false;
  if (!func->body.empty()) {
    hasReturnAtEnd =
        dynamic_cast<const ReturnStmt *>(func->body.back().get()) != nullptr;
  }

  if (!hasReturnAtEnd) {
    if (func->name == "main") {
      output += indent() + "return 0;\n";
    } else if (actualReturnType == ValueType::NUMBER) {
      output += indent() + "return 0.0;\n";
    } else if (actualReturnType == ValueType::STRING) {
      output += indent() + "return \"\";\n";
    } else if (actualReturnType == ValueType::BOOL) {
      output += indent() + "return 0;\n";
    } else {
      output += indent() + "return 0;\n";
    }
  }

  indentLevel = savedIndent;
  output += "}\n";

  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
  currentFunctionParams = savedFunctionParams;
}

void Compiler::compileNestedFunction(
    const FunctionDecl *func,
    const std::vector<std::pair<std::string, ValueType>> &parentParams) {

  output += "static ";

  std::string savedFunction = currentFunction;
  currentFunction = func->name;
  auto savedSymbolTable = symbolTable;

  for (const auto &param : parentParams) {
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }
    symbolTable[param.first] = {paramType, false, false, true, false};
  }

  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }
    
    bool isOptional = (i < func->parameterOptionals.size()) ? func->parameterOptionals[i] : false;
    
    symbolTable[param.first] = {paramType, false, true, isOptional, false, false, ""};
  }

  ValueType actualReturnType = func->returnType;
  if (actualReturnType == ValueType::INFERRED) {
    actualReturnType = ValueType::NUMBER;
  }

  std::string returnType = getCType(actualReturnType);

  output += returnType + " " + func->name + "(";

  size_t paramCount = 0;

  for (const auto &param : parentParams) {
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }
    std::string paramCType = getCType(paramType);
    output += paramCType + " " + param.first;
    paramCount++;

    if (paramCount < parentParams.size() + func->parameters.size()) {
      output += ", ";
    }
  }

  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }
    std::string paramCType = getCType(paramType);
    output += paramCType + " " + param.first;
    paramCount++;

    if (i < func->parameters.size() - 1) {
      output += ", ";
    }
  }

  output += ") {\n";

  int savedIndent = indentLevel;
  indentLevel = 1;

  for (const auto &stmt : func->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  bool hasReturnAtEnd = false;
  if (!func->body.empty()) {
    hasReturnAtEnd =
        dynamic_cast<const ReturnStmt *>(func->body.back().get()) != nullptr;
  }

  if (!hasReturnAtEnd) {
    if (actualReturnType == ValueType::NUMBER) {
      output += indent() + "return 0.0;\n";
    } else if (actualReturnType == ValueType::STRING) {
      output += indent() + "return \"\";\n";
    } else if (actualReturnType == ValueType::BOOL) {
      output += indent() + "return 0;\n";
    } else {
      output += indent() + "return 0;\n";
    }
  }

  indentLevel = savedIndent;
  output += "}\n";

  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
}

void Compiler::compileLambdaExpr(const LambdaExpr *lambda,
                                 std::string &funcName) {
  if (funcName.empty()) {
    funcName = generateUniqueName("func");
  }

  std::string savedFunction = currentFunction;
  currentFunction = funcName;

  auto savedFunctionParams = currentFunctionParams;
  currentFunctionParams = lambda->parameters;
  auto savedSymbolTable = symbolTable;

  for (size_t i = 0; i < lambda->parameters.size(); i++) {
    const auto &param = lambda->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }

    bool isOptional = (i < lambda->parameterOptionals.size()) ? lambda->parameterOptionals[i] : false;

    symbolTable[param.first] = {paramType, false, true, isOptional, false, false, ""};
  }

  ValueType actualReturnType = lambda->returnType;

  if (actualReturnType == ValueType::INFERRED) {
    for (const auto &stmt : lambda->body) {
      if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
        if (ret->value) {
          actualReturnType = inferExprType(ret->value.get());
          break;
        }
      }
    }

    if (actualReturnType == ValueType::INFERRED) {
      actualReturnType = ValueType::NUMBER;
    }
  }

  std::string returnType = getCType(actualReturnType);

  output += "static " + returnType + " " + funcName + "(";

  for (size_t i = 0; i < lambda->parameters.size(); i++) {
    const auto &param = lambda->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }

    std::string paramCType = getCType(paramType);
    output += paramCType + " " + param.first;
    if (i < lambda->parameters.size() - 1) {
      output += ", ";
    }
  }

  output += ") {\n";

  int savedIndent = indentLevel;
  indentLevel = 1;

  for (const auto &stmt : lambda->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  bool hasReturnAtEnd = false;
  if (!lambda->body.empty()) {
    hasReturnAtEnd =
        dynamic_cast<const ReturnStmt *>(lambda->body.back().get()) != nullptr;
  }

  if (!hasReturnAtEnd) {
    if (actualReturnType == ValueType::NUMBER) {
      output += indent() + "return 0.0;\n";
    } else if (actualReturnType == ValueType::STRING) {
      output += indent() + "return \"\";\n";
    } else if (actualReturnType == ValueType::BOOL) {
      output += indent() + "return 0;\n";
    } else {
      output += indent() + "return 0;\n";
    }
  }

  indentLevel = savedIndent;
  output += "}\n\n";

  FunctionInfo funcInfo;
  funcInfo.name = funcName;
  funcInfo.parameters = lambda->parameters;
  funcInfo.parameterOptionals = lambda->parameterOptionals;
  funcInfo.returnType = actualReturnType;
  funcInfo.isGlobal = false;
  funcInfo.nestedFunctions = {};
  functionTable[funcName] = funcInfo;

  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
  currentFunctionParams = savedFunctionParams;
}

} // namespace HolyLua