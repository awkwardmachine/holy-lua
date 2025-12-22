#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compileClassDecl(const ClassDecl *decl) {
  auto &info = classTable[decl->name];
  info.name = decl->name;
  info.fields = decl->fields;
  
  for (const auto &field : decl->fields) {
    info.fieldInfo[field.name] = {field.type, field.visibility};
  }
  
  for (const auto &method : decl->methods) {
    info.methodInfo[method.name] = {method.returnType, method.visibility};
    
    if (method.isStatic && method.returnType == ValueType::INFERRED) {
      if (method.name == "new" || method.name.find("create") != std::string::npos ||
          method.name.find("New") != std::string::npos) {
        info.methodInfo[method.name] = {ValueType::STRUCT, method.visibility};
      }
    }
  }
  
  if (decl->constructor) {
    info.hasConstructor = true;
    info.constructorParams = decl->constructor->parameters;
    info.constructorParamOptionals = decl->constructor->parameterOptionals;
    info.constructorParamTypeNames = decl->constructor->parameterTypeNames;
    info.methodInfo["__init"] = {ValueType::INFERRED, Visibility::PUBLIC};
  }
  
  // generate struct definition
  std::string structDef = "typedef struct {\n";
  
  for (const auto &field : decl->fields) {
    if (!field.isStatic) {
      std::string fieldType;
      if (field.type == ValueType::STRUCT) {
        fieldType = field.structTypeName;
      } else if (field.type == ValueType::ENUM) {
        fieldType = field.structTypeName;
      } else {
        fieldType = getCType(field.type);
      }
      structDef += "    " + fieldType + " " + field.name + ";\n";
    }
  }
  
  structDef += "} " + decl->name + ";\n\n";
  output += structDef;
  
  // generate static field declarations
  for (const auto &field : decl->fields) {
    if (field.isStatic) {
      std::string fieldType;
      if (field.type == ValueType::ENUM) {
        fieldType = field.structTypeName;
      } else {
        fieldType = getCType(field.type);
      }
      
      std::string staticFieldDecl = "";
      
      if (field.isConst) {
        staticFieldDecl += "static const ";
      } else {
        staticFieldDecl += "static ";
      }
      
      if (field.visibility == Visibility::PRIVATE) {
        staticFieldDecl += "/* private */ ";
      }
      
      staticFieldDecl += fieldType + " " + decl->name + "_" + field.name;
      
      if (field.hasDefault) {
        staticFieldDecl += " = ";
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, int64_t>) {
                if (field.type == ValueType::ENUM) {
                  staticFieldDecl += std::to_string(arg);
                } else {
                  staticFieldDecl += std::to_string(arg) + ".0";
                }
              } else if constexpr (std::is_same_v<T, double>) {
                staticFieldDecl += doubleToString(arg);
              } else if constexpr (std::is_same_v<T, std::string>) {
                staticFieldDecl += "\"" + arg + "\"";
              } else if constexpr (std::is_same_v<T, bool>) {
                staticFieldDecl += arg ? "1" : "0";
              } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                if (field.type == ValueType::ENUM) {
                  staticFieldDecl += "-1";
                } else if (field.type == ValueType::NUMBER) {
                  staticFieldDecl += "HL_NIL_NUMBER";
                } else if (field.type == ValueType::STRING) {
                  staticFieldDecl += "NULL";
                } else if (field.type == ValueType::BOOL) {
                  staticFieldDecl += "-1";
                } else {
                  staticFieldDecl += "0";
                }
              }
            },
            field.defaultValue);
      }
      
      staticFieldDecl += ";\n";
      output += staticFieldDecl;
    }
  }
  
  output += "\n";
  
  // Compile constructor
  if (decl->constructor) {
    std::string savedOutput = output;
    output = "";
    compileConstructor(decl->name, *decl->constructor);
    savedOutput += output;
    output = savedOutput;
  }
  
  // Compile methods
  for (const auto &method : decl->methods) {
    std::string savedOutput = output;
    output = "";
    compileMethod(decl->name, method);
    savedOutput += output;
    output = savedOutput;
  }
}

std::string Compiler::compileClassInstantiation(const ClassInstantiation *expr) {
  if (!classTable.count(expr->className)) {
    error("Class '" + expr->className + "' not defined", expr->line);
    return "";
  }
  
  const auto &classInfo = classTable[expr->className];
  
  if (!classInfo.hasConstructor) {
    error("Class '" + expr->className + "' has no constructor", expr->line);
    return "";
  }
  
  std::string result = expr->className + "_new(";
  
  size_t providedArgs = expr->arguments.size();
  size_t totalParams = classInfo.constructorParams.size();
  
  // count required parameters
  size_t requiredParams = 0;
  for (size_t i = 0; i < totalParams; i++) {
    if (i >= classInfo.constructorParamOptionals.size() || 
        !classInfo.constructorParamOptionals[i]) {
      requiredParams++;
    } else {
      break;
    }
  }
  
  // check if we have enough arguments
  if (providedArgs < requiredParams) {
    error("Constructor for '" + expr->className + "' requires at least " + 
          std::to_string(requiredParams) + " argument(s), but got " + 
          std::to_string(providedArgs), expr->line);
    return "";
  }
  
  if (providedArgs > totalParams) {
    error("Constructor for '" + expr->className + "' expects at most " + 
          std::to_string(totalParams) + " argument(s), but got " + 
          std::to_string(providedArgs), expr->line);
    return "";
  }
  
  // compile provided arguments
  for (size_t i = 0; i < providedArgs; i++) {
    bool isNilArg = dynamic_cast<const NilExpr *>(expr->arguments[i].get()) != nullptr;
    
    if (isNilArg && i < totalParams && 
        i < classInfo.constructorParamOptionals.size() && 
        classInfo.constructorParamOptionals[i]) {
      const auto &param = classInfo.constructorParams[i];
      ValueType paramType = param.second;
      
      if (paramType == ValueType::STRING) {
        result += "(char*)0";
      } else if (paramType == ValueType::NUMBER) {
        result += "HL_NIL_NUMBER";
      } else if (paramType == ValueType::BOOL) {
        result += "-1";
      } else if (paramType == ValueType::ENUM) {
        result += "-1";
      } else if (paramType == ValueType::STRUCT) {
        result += "HL_NIL_NUMBER";
      } else {
        result += "HL_NIL_NUMBER";
      }
    } else {
      result += compileExpr(expr->arguments[i].get());
    }
    
    if (i < providedArgs - 1 || providedArgs < totalParams) {
      result += ", ";
    }
  }
  
  // fill in nil values for omitted optional parameters
  for (size_t i = providedArgs; i < totalParams; i++) {
    const auto &param = classInfo.constructorParams[i];
    ValueType paramType = param.second;

    if (paramType == ValueType::STRING) {
      result += "(char*)0";
    } else if (paramType == ValueType::NUMBER) {
      result += "HL_NIL_NUMBER";
    } else if (paramType == ValueType::BOOL) {
      result += "-1";
    } else if (paramType == ValueType::ENUM) {
      result += "-1";
    } else if (paramType == ValueType::STRUCT) {
      result += "HL_NIL_NUMBER";
    } else {
      result += "HL_NIL_NUMBER";
    }
    
    if (i < totalParams - 1) {
      result += ", ";
    }
  }
  
  result += ")";
  return result;
}

std::string Compiler::compileSelfExpr(const SelfExpr *expr) {
  (void)expr;
  
  if (currentFunction.find("___init") != std::string::npos) {
    return "self";
  } else {
    return "self";
  }
}

} // namespace HolyLua