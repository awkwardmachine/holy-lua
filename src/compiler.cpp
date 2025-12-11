#include "common.h"
#include "compiler.h"
#include <iostream>
#include <sstream>

namespace HolyLua {

Compiler::Compiler(const std::string &source) : source(source) {
  initSourceLines();
}

void Compiler::initSourceLines() {
  std::stringstream ss(source);
  std::string line;
  while (std::getline(ss, line)) {
    sourceLines.push_back(line);
  }
}

void Compiler::error(const std::string &msg, int line) {
  std::cerr << "\033[1;31mError:\033[0m " << msg << "\n";
  showErrorContext(line);
}

void Compiler::showErrorContext(int line) {
  if (line < 1 || line > static_cast<int>(sourceLines.size()))
    return;

  int lineIdx = line - 1;

  if (lineIdx > 0) {
    std::cerr << "  " << (lineIdx) << " | " << sourceLines[lineIdx - 1] << "\n";
  }

  std::cerr << "\033[1;33m> " << line << " | " << sourceLines[lineIdx]
            << "\033[0m\n";

  if (lineIdx < static_cast<int>(sourceLines.size()) - 1) {
    std::cerr << "  " << (lineIdx + 2) << " | " << sourceLines[lineIdx + 1]
              << "\n";
  }
  std::cerr << "\n";
}

std::string Compiler::getCTypeForStruct(const std::string &structName) {
  return structName;
}

std::string Compiler::indent() { return std::string(indentLevel * 4, ' '); }

std::string Compiler::compile(const Program &program) {
  std::string globalDecls = "";
  std::string mainBody = "";
  std::string functionDecls = "";
  std::string structDefinitions = "";
  nestedFunctionDecls = "";
  bool hasErrors = false;
  bool hasMainFunction = false;

  // first: compile all struct declarations
  for (const auto &stmt : program.statements) {
    if (auto *structDecl = dynamic_cast<StructDecl *>(stmt.get())) {
      compileStructDecl(structDecl);
    }
  }

  // collect all struct definitions first
  for (const auto &pair : structDefs) {
    structDefinitions += pair.second;
  }

  // second: collect function info
  for (const auto &stmt : program.statements) {
    if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
      // create FunctionInfo object explicitly
      FunctionInfo funcInfo;
      funcInfo.name = func->name;
      funcInfo.parameters = func->parameters;
      funcInfo.parameterOptionals = func->parameterOptionals;
      funcInfo.returnType = func->returnType;
      funcInfo.isGlobal = func->isGlobal;
      funcInfo.nestedFunctions = {};

      functionTable[func->name] = funcInfo;

      if (func->name == "main") {
        hasMainFunction = true;
      }
    }
  }

  // third: collect all top-level variables as global declarations
  std::vector<std::pair<std::string, std::string>> deferredInitializations;
  
  for (const auto &stmt : program.statements) {
    if (auto *decl = dynamic_cast<VarDecl *>(stmt.get())) {
      ValueType actualType = decl->type;

      if (actualType == ValueType::INFERRED && decl->hasValue) {
        actualType = inferExprType(decl->value.get());
      }

      // check if this is a lambda assignment
      if (decl->hasValue) {
        if (auto *lambda = dynamic_cast<const LambdaExpr *>(decl->value.get())) {
          std::string funcName = decl->name;
          std::string savedOutput = output;
          output = "";

          compileLambdaExpr(lambda, funcName);
          nestedFunctionDecls += output;
          output = savedOutput;

          symbolTable[decl->name] = {actualType, decl->isConst, true, true, decl->isOptional};
          continue;
        }
      }

      std::string ctype = getCType(actualType);

      if (actualType == ValueType::STRUCT && decl->hasValue) {
        if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
          ctype = structCons->structName;
        }
      }

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
          }
        } else {
          if (containsVariables(decl->value.get())) {
            canBeGlobalInitializer = false;
          } else {
            initExpr = compileExpr(decl->value.get(), actualType, true);
          }
        }
      }

      if (decl->isConst) globalDecls += "const ";
      globalDecls += ctype + " " + decl->name;
      
      if (canBeGlobalInitializer && !initExpr.empty()) {
        globalDecls += " = " + initExpr + ";\n";
      } else {
        globalDecls += ";\n";
        
        if (decl->hasValue) {
          std::string initCode;
          if (actualType == ValueType::STRUCT) {
            if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
              initCode = decl->name + " = " + compileStructConstructor(structCons) + ";";
            } else {
              initCode = decl->name + " = " + compileExpr(decl->value.get(), actualType, false) + ";";
            }
          } else {
            initCode = decl->name + " = " + compileExpr(decl->value.get(), actualType, false) + ";";
          }
          deferredInitializations.push_back({decl->name, initCode});
        }
      }

      symbolTable[decl->name] = {actualType, decl->isConst, true, true, decl->isOptional};
    }
  }

  // fourth: compile top-level statements
  if (!hasMainFunction) {
    for (const auto &stmt : program.statements) {
      if (dynamic_cast<StructDecl *>(stmt.get())) {
        // skip struct declarations, already processed
      } else if (dynamic_cast<VarDecl *>(stmt.get())) {
        // skip variable declarations
      } else if (dynamic_cast<FunctionDecl *>(stmt.get())) {
        // skip functions in this pass
      } else {
        output = "";
        compileStatement(stmt.get());
        if (output.empty()) {
          hasErrors = true;
          break;
        }
        mainBody += output;
      }
    }
  }

  // fifth: compile function definitions
  for (const auto &stmt : program.statements) {
    if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
      output = "";
      compileFunctionDecl(func);
      if (output.empty()) {
        hasErrors = true;
        break;
      }
      functionDecls += output + "\n";
    }
  }

  if (hasErrors) {
    return "";
  }

  // generate output with Holy Lua API
  output = "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n";
  output += "#include <math.h>\n";
  output += "#include \"api/holylua_api.h\"\n\n";
  output += structDefinitions;
  output += globalDecls + "\n";

  if (!nestedFunctionDecls.empty()) {
    output += nestedFunctionDecls + "\n";
  }

  // move all function declarations to the end
  output += functionDecls + "\n";

  if (hasMainFunction) {
    return output;
  }

  // generate main function
  output += "int main() {\n";
  indentLevel = 1;
  std::string savedOutput = output;
  output = "";


  for (const auto &init : deferredInitializations) {
    output += indent() + init.second + "\n";
  }

  for (const auto &stmt : program.statements) {
    if (dynamic_cast<StructDecl *>(stmt.get())) {
      // skip struct declarations
    } else if (dynamic_cast<VarDecl *>(stmt.get())) {
      // skip variable declarations (already handled above)
    } else if (dynamic_cast<FunctionDecl *>(stmt.get())) {
      // skip functions in main
    } else {
      compileStatement(stmt.get());
      savedOutput += output;
      output = "";
    }
  }

  indentLevel = 0;
  output = savedOutput + "    return 0;\n}\n";

  return output;
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

std::string Compiler::compileStructInitializer(const StructConstructor *expr) {
  if (structTable.find(expr->structName) == structTable.end()) {
    error("Struct '" + expr->structName + "' not defined", expr->line);
    return "";
  }

  const auto &structInfo = structTable[expr->structName];

  // generate C struct initializer
  std::string result = "{";

  if (expr->useDefaults) {
    // use all defaults
    bool first = true;
    for (const auto &field : structInfo.fields) {
      if (!first)
        result += ", ";
      first = false;

      if (field.hasDefault) {
        // use the stored default value
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, int64_t>) {
                result += std::to_string(arg) + ".0";
              } else if constexpr (std::is_same_v<T, double>) {
                result += std::to_string(arg);
              } else if constexpr (std::is_same_v<T, std::string>) {
                result += "\"" + arg + "\"";
              } else if constexpr (std::is_same_v<T, bool>) {
                result += arg ? "1" : "0";
              } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                if (field.type == ValueType::NUMBER) {
                  result += "HL_NIL_NUMBER";
                } else if (field.type == ValueType::STRING) {
                  result += "NULL";
                } else if (field.type == ValueType::BOOL) {
                  result += "-1";
                } else {
                  result += "0";
                }
              }
            },
            field.defaultValue);
      } else if (field.isOptional) {
        // set to nil value
        if (field.type == ValueType::NUMBER) {
          result += "HL_NIL_NUMBER";
        } else if (field.type == ValueType::STRING) {
          result += "NULL";
        } else if (field.type == ValueType::BOOL) {
          result += "-1";
        } else {
          result += "0";
        }
      } else {
        // set to zero value
        if (field.type == ValueType::NUMBER) {
          result += "0.0";
        } else if (field.type == ValueType::STRING) {
          result += "\"\"";
        } else if (field.type == ValueType::BOOL) {
          result += "0";
        } else {
          result += "0";
        }
      }
    }
  } else if (!expr->positionalArgs.empty()) {
    // positional arguments
    if (expr->positionalArgs.size() > structInfo.fields.size()) {
      error("Too many arguments for struct '" + expr->structName + "'",
            expr->line);
      return "";
    }

    bool first = true;
    for (size_t i = 0; i < structInfo.fields.size(); i++) {
      if (!first)
        result += ", ";
      first = false;

      if (i < expr->positionalArgs.size()) {
        const auto &field = structInfo.fields[i];
        std::string argValue =
            compileExpr(expr->positionalArgs[i].get(), field.type, true);

        // check if the argument is a simple variable reference that can be used
        // in global initializer
        if (auto *varExpr =
                dynamic_cast<const VarExpr *>(expr->positionalArgs[i].get())) {
          result += varExpr->name;
        } else {
          result += argValue;
        }
      } else {
        const auto &field = structInfo.fields[i];
        if (field.hasDefault) {
          std::visit(
              [&](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                  result += std::to_string(arg) + ".0";
                } else if constexpr (std::is_same_v<T, double>) {
                  result += std::to_string(arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                  result += "\"" + arg + "\"";
                } else if constexpr (std::is_same_v<T, bool>) {
                  result += arg ? "1" : "0";
                } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                  if (field.type == ValueType::NUMBER) {
                    result += "HL_NIL_NUMBER";
                  } else if (field.type == ValueType::STRING) {
                    result += "NULL";
                  } else if (field.type == ValueType::BOOL) {
                    result += "-1";
                  } else {
                    result += "0";
                  }
                }
              },
              field.defaultValue);
        } else if (field.isOptional) {
          if (field.type == ValueType::NUMBER) {
            result += "HL_NIL_NUMBER";
          } else if (field.type == ValueType::STRING) {
            result += "NULL";
          } else if (field.type == ValueType::BOOL) {
            result += "-1";
          } else {
            result += "0";
          }
        } else {
          if (field.type == ValueType::NUMBER) {
            result += "0.0";
          } else if (field.type == ValueType::STRING) {
            result += "\"\"";
          } else if (field.type == ValueType::BOOL) {
            result += "0";
          } else {
            result += "0";
          }
        }
      }
    }
  } else {
    std::map<std::string, std::string> namedArgValues;
    for (const auto &namedArg : expr->namedArgs) {
      const auto &fieldName = namedArg.first;
      const auto &argExpr = namedArg.second;

      // find the field type
      ValueType fieldType = ValueType::INFERRED;
      for (const auto &field : structInfo.fields) {
        if (field.name == fieldName) {
          fieldType = field.type;
          break;
        }
      }

      namedArgValues[fieldName] = compileExpr(argExpr.get(), fieldType, true);
    }

    bool first = true;
    for (const auto &field : structInfo.fields) {
      if (!first)
        result += ", ";
      first = false;

      if (namedArgValues.find(field.name) != namedArgValues.end()) {
        const Expr *argExpr = nullptr;
        for (const auto &namedArg : expr->namedArgs) {
          if (namedArg.first == field.name) {
            argExpr = namedArg.second.get();
            break;
          }
        }

        if (argExpr) {
          if (auto *varExpr = dynamic_cast<const VarExpr *>(argExpr)) {
            result += varExpr->name;
          } else {
            result += namedArgValues[field.name];
          }
        } else {
          result += namedArgValues[field.name];
        }
      } else if (field.hasDefault) {
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, int64_t>) {
                result += std::to_string(arg) + ".0";
              } else if constexpr (std::is_same_v<T, double>) {
                result += std::to_string(arg);
              } else if constexpr (std::is_same_v<T, std::string>) {
                result += "\"" + arg + "\"";
              } else if constexpr (std::is_same_v<T, bool>) {
                result += arg ? "1" : "0";
              } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                if (field.type == ValueType::NUMBER) {
                  result += "HL_NIL_NUMBER";
                } else if (field.type == ValueType::STRING) {
                  result += "NULL";
                } else if (field.type == ValueType::BOOL) {
                  result += "-1";
                } else {
                  result += "0";
                }
              }
            },
            field.defaultValue);
      } else if (field.isOptional) {
        if (field.type == ValueType::NUMBER) {
          result += "HL_NIL_NUMBER";
        } else if (field.type == ValueType::STRING) {
          result += "NULL";
        } else if (field.type == ValueType::BOOL) {
          result += "-1";
        } else {
          result += "0";
        }
      } else {
        if (field.type == ValueType::NUMBER) {
          result += "0.0";
        } else if (field.type == ValueType::STRING) {
          result += "\"\"";
        } else if (field.type == ValueType::BOOL) {
          result += "0";
        } else {
          result += "0";
        }
      }
    }
  }

  result += "}";
  return result;
}

void Compiler::compileStatement(const ASTNode *node) {
  if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
    compileVarDecl(decl);
  } else if (auto *func = dynamic_cast<const FunctionDecl *>(node)) {
    compileFunctionDecl(func);
  } else if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
    compileReturnStmt(ret);
  } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
    compileAssignment(assign);
  } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
    compilePrintStmt(print);
  } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(node)) {
    compileIfStmt(ifStmt);
  } else if (auto *call = dynamic_cast<const FunctionCall *>(node)) {
    std::string result = compileFunctionCall(call);
    if (!result.empty()) {
      output += indent() + result + ";\n";
    }
  } else if (auto *inlineC = dynamic_cast<const InlineCStmt *>(node)) {
    compileInlineCStmt(inlineC);
  } else if (auto *whileStmt = dynamic_cast<const WhileStmt *>(node)) {
    compileWhileStmt(whileStmt);
  } else if (auto *forStmt = dynamic_cast<const ForStmt *>(node)) {
    compileForStmt(forStmt);
  } else if (auto *repeatStmt = dynamic_cast<const RepeatStmt *>(node)) {
    compileRepeatStmt(repeatStmt);
  } else if (auto *structDecl = dynamic_cast<const StructDecl *>(node)) {
    compileStructDecl(structDecl);
  }
}

void Compiler::compileStructDecl(const StructDecl *decl) {
  // store struct information for later use
  StructInfo info;
  info.name = decl->name;
  info.fields = decl->fields;

  for (const auto &field : decl->fields) {
    info.fieldTypes[field.name] = {field.type, field.isOptional};
  }

  structTable[decl->name] = info;

  // generate C struct definition
  std::string structDef;
  structDef += "typedef struct {\n";
  for (const auto &field : info.fields) {
    std::string fieldType;
    if (field.type == ValueType::STRUCT) {
      if (!field.structTypeName.empty()) {
        fieldType = field.structTypeName;
      } else {
        error("Struct field '" + field.name + "' has unknown struct type", 0);
        fieldType = "void*";
      }
    } else {
      fieldType = getCType(field.type);
    }
    structDef += "    " + fieldType + " " + field.name + ";\n";
  }
  structDef += "} " + decl->name + ";\n\n";
  structDefs[decl->name] = structDef;
}

std::string Compiler::compileStructConstructor(const StructConstructor *expr) {
  if (structTable.find(expr->structName) == structTable.end()) {
    error("Struct '" + expr->structName + "' not defined", expr->line);
    return "";
  }

  const auto &structInfo = structTable[expr->structName];

  // generate constructor call
  std::string result = "(" + expr->structName + "){";

  if (expr->useDefaults) {
    // use all defaults
    bool first = true;
    for (const auto &field : structInfo.fields) {
      if (!first)
        result += ", ";
      first = false;

      if (field.hasDefault) {
        // use the stored default value
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, int64_t>) {
                result += std::to_string(arg) + ".0";
              } else if constexpr (std::is_same_v<T, double>) {
                result += std::to_string(arg);
              } else if constexpr (std::is_same_v<T, std::string>) {
                result += "\"" + arg + "\"";
              } else if constexpr (std::is_same_v<T, bool>) {
                result += arg ? "1" : "0";
              } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                if (field.type == ValueType::NUMBER) {
                  result += "HL_NIL_NUMBER";
                } else if (field.type == ValueType::STRING) {
                  result += "NULL";
                } else if (field.type == ValueType::BOOL) {
                  result += "-1";
                } else {
                  result += "0";
                }
              }
            },
            field.defaultValue);
      } else if (field.isOptional) {
        // set to nil value
        if (field.type == ValueType::NUMBER) {
          result += "HL_NIL_NUMBER";
        } else if (field.type == ValueType::STRING) {
          result += "NULL";
        } else if (field.type == ValueType::BOOL) {
          result += "-1";
        } else {
          result += "0";
        }
      } else {
        // set to zero value
        if (field.type == ValueType::NUMBER) {
          result += "0.0";
        } else if (field.type == ValueType::STRING) {
          result += "\"\"";
        } else if (field.type == ValueType::BOOL) {
          result += "0";
        } else {
          result += "0";
        }
      }
    }
  } else if (!expr->positionalArgs.empty()) {
    // positional arguments
    if (expr->positionalArgs.size() > structInfo.fields.size()) {
      error("Too many arguments for struct '" + expr->structName + "'",
            expr->line);
      return "";
    }

    bool first = true;
    for (size_t i = 0; i < structInfo.fields.size(); i++) {
      if (!first)
        result += ", ";
      first = false;

      if (i < expr->positionalArgs.size()) {
        const auto &field = structInfo.fields[i];
        result += compileExpr(expr->positionalArgs[i].get(), field.type);
      } else {
        const auto &field = structInfo.fields[i];
        if (field.hasDefault) {
          std::visit(
              [&](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                  result += std::to_string(arg) + ".0";
                } else if constexpr (std::is_same_v<T, double>) {
                  result += std::to_string(arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                  result += "\"" + arg + "\"";
                } else if constexpr (std::is_same_v<T, bool>) {
                  result += arg ? "1" : "0";
                } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                  if (field.type == ValueType::NUMBER) {
                    result += "HL_NIL_NUMBER";
                  } else if (field.type == ValueType::STRING) {
                    result += "NULL";
                  } else if (field.type == ValueType::BOOL) {
                    result += "-1";
                  } else {
                    result += "0";
                  }
                }
              },
              field.defaultValue);
        } else if (field.isOptional) {
          if (field.type == ValueType::NUMBER) {
            result += "HL_NIL_NUMBER";
          } else if (field.type == ValueType::STRING) {
            result += "NULL";
          } else if (field.type == ValueType::BOOL) {
            result += "-1";
          } else {
            result += "0";
          }
        } else {
          if (field.type == ValueType::NUMBER) {
            result += "0.0";
          } else if (field.type == ValueType::STRING) {
            result += "\"\"";
          } else if (field.type == ValueType::BOOL) {
            result += "0";
          } else {
            result += "0";
          }
        }
      }
    }
  } else {
    // named arguments
    std::map<std::string, bool> argProvided;
    for (const auto &namedArg : expr->namedArgs) {
      argProvided[namedArg.first] = true;
    }

    bool first = true;
    for (const auto &field : structInfo.fields) {
      if (!first)
        result += ", ";
      first = false;

      if (argProvided[field.name]) {
        for (const auto &namedArg : expr->namedArgs) {
          if (namedArg.first == field.name) {
            result += compileExpr(namedArg.second.get(), field.type);
            break;
          }
        }
      } else if (field.hasDefault) {
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, int64_t>) {
                result += std::to_string(arg) + ".0";
              } else if constexpr (std::is_same_v<T, double>) {
                result += std::to_string(arg);
              } else if constexpr (std::is_same_v<T, std::string>) {
                result += "\"" + arg + "\"";
              } else if constexpr (std::is_same_v<T, bool>) {
                result += arg ? "1" : "0";
              } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                if (field.type == ValueType::NUMBER) {
                  result += "HL_NIL_NUMBER";
                } else if (field.type == ValueType::STRING) {
                  result += "NULL";
                } else if (field.type == ValueType::BOOL) {
                  result += "-1";
                } else {
                  result += "0";
                }
              }
            },
            field.defaultValue);
      } else if (field.isOptional) {
        if (field.type == ValueType::NUMBER) {
          result += "HL_NIL_NUMBER";
        } else if (field.type == ValueType::STRING) {
          result += "NULL";
        } else if (field.type == ValueType::BOOL) {
          result += "-1";
        } else {
          result += "0";
        }
      } else {
        if (field.type == ValueType::NUMBER) {
          result += "0.0";
        } else if (field.type == ValueType::STRING) {
          result += "\"\"";
        } else if (field.type == ValueType::BOOL) {
          result += "0";
        } else {
          result += "0";
        }
      }
    }
  }

  result += "}";
  return result;
}

std::string Compiler::compileFieldAccess(const FieldAccessExpr *expr) {
  std::string object = compileExpr(expr->object.get());
  return object + "." + expr->fieldName;
}

void Compiler::compileRepeatStmt(const RepeatStmt *repeatStmt) {
  pushScope();

  // do while loop in C
  output += indent() + "do {\n";

  // compile loop body
  indentLevel++;

  for (const auto &stmt : repeatStmt->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  indentLevel--;
  output += indent() + "} while (!(" +
            compileExpr(repeatStmt->condition.get()) + "));\n";

  popScope();
}

void Compiler::compileForStmt(const ForStmt *forStmt) {
  pushScope();

  // declare and initialize the loop variable
  output += indent() + "for (double " + forStmt->varName + " = ";
  output += compileExpr(forStmt->start.get());
  output += "; " + forStmt->varName + " <= ";
  output += compileExpr(forStmt->end.get());
  output += "; " + forStmt->varName + " += ";

  if (forStmt->step) {
    output += compileExpr(forStmt->step.get());
  } else {
    output += "1.0";
  }

  output += ") {\n";

  // add variable to symbol table
  symbolTable[forStmt->varName] = {ValueType::NUMBER, false, false, true,
                                   false};

  // compile loop body
  indentLevel++;

  for (const auto &stmt : forStmt->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  indentLevel--;
  output += indent() + "}\n";

  // remove variable from current scope after loop ends
  symbolTable.erase(forStmt->varName);
  popScope();
}

void Compiler::compileWhileStmt(const WhileStmt *whileStmt) {
  // generate the while condition
  output +=
      indent() + "while (" + compileExpr(whileStmt->condition.get()) + ") {\n";

  pushScope();
  indentLevel++;

  // compile loop body
  for (const auto &stmt : whileStmt->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  // exit loop scope
  indentLevel--;
  popScope();
  output += indent() + "}\n";
}

void Compiler::compileInlineCStmt(const InlineCStmt *stmt) {
  // add the C code directly with proper indentation
  output += indent() + stmt->cCode + "\n";
}

void Compiler::compileFunctionDecl(const FunctionDecl *func) {
  // save current function context
  std::string savedFunction = currentFunction;
  currentFunction = func->name;

  // Save parent function parameters
  auto savedFunctionParams = currentFunctionParams;
  currentFunctionParams = func->parameters;

  // save global symbol table and create new scope
  auto savedSymbolTable = symbolTable;

  // track parent function parameters for nested functions
  std::vector<std::pair<std::string, ValueType>> parentParams;
  if (!func->isGlobal) {
    // this is a nested function

  } else {
    // top-level function, collect its parameters for potential nested
    // functions
    parentParams = func->parameters;
  }

  // add parameters to symbol table first
  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }

    symbolTable[param.first] = {paramType, false, false, true, false};
  }

  // first pass: collect and compile nested function declarations
  // these will be lifted to file scope as static functions
  for (const auto &stmt : func->body) {
    if (auto *nestedFunc = dynamic_cast<const FunctionDecl *>(stmt.get())) {
      // add nested function to function table
      FunctionInfo nestedInfo;
      nestedInfo.name = nestedFunc->name;

      // nested function needs parent's params + its own params for closure
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

      // add to symbol table so it can be called
      symbolTable[nestedFunc->name] = {ValueType::INFERRED, false, true, false,
                                       false};

      // compile nested function
      std::string savedOutput = output;
      output = "";

      compileNestedFunction(nestedFunc, func->parameters);

      nestedFunctionDecls += output + "\n";
      output = savedOutput;
    }
  }

  ValueType actualReturnType = func->returnType;

  // if return type is inferred, try to determine from context
  if (actualReturnType == ValueType::INFERRED) {
    // check function body for return statements
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

    // if still INFERRED after checking all returns, default based on the
    // expression type analysis
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

  // update function table with actual return type
  if (func->isGlobal) {
    functionTable[func->name].returnType = actualReturnType;
  }

  std::string returnType = getCType(actualReturnType);

  // generate parent function signature
  output += returnType + " " + func->name + "(";

  // generate parameter list
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

  // compile function body
  for (const auto &stmt : func->body) {
    // skip nested function declarations, already compiled as static functions
    if (dynamic_cast<const FunctionDecl *>(stmt.get())) {
      continue;
    }

    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  // check if function ends with return statement
  bool hasReturnAtEnd = false;
  if (!func->body.empty()) {
    hasReturnAtEnd =
        dynamic_cast<const ReturnStmt *>(func->body.back().get()) != nullptr;
  }

  // if no return statement at end, add default return based on return type
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

  // restore symbol table and function context
  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
  currentFunctionParams = savedFunctionParams;
}

void Compiler::compileNestedFunction(
    const FunctionDecl *func,
    const std::vector<std::pair<std::string, ValueType>> &parentParams) {

  // nested functions become static file-scope functions
  output += "static ";

  // save context
  std::string savedFunction = currentFunction;
  currentFunction = func->name;
  auto savedSymbolTable = symbolTable;

  // add parent parameters to symbol table
  for (const auto &param : parentParams) {
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }
    symbolTable[param.first] = {paramType, false, false, true, false};
  }

  // add nested function's own parameters
  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }
    symbolTable[param.first] = {paramType, false, false, true, false};
  }

  // determine return type
  ValueType actualReturnType = func->returnType;
  if (actualReturnType == ValueType::INFERRED) {
    actualReturnType = ValueType::NUMBER; // default
  }

  std::string returnType = getCType(actualReturnType);

  // generate function signature
  output += returnType + " " + func->name + "(";

  size_t paramCount = 0;

  // parent parameters come first
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

  // then nested function's own parameters
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

  // compile function body
  for (const auto &stmt : func->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  // add default return if needed
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

  // restore context
  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
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

void Compiler::compileReturnStmt(const ReturnStmt *ret) {
  if (currentFunction.empty()) {
    error("Return statement outside of function", ret->line);
    output = "";
    return;
  }

  output += indent() + "return";

  if (ret->value) {
    // get the function's return type from the function table
    ValueType returnType = ValueType::INFERRED;
    if (functionTable.count(currentFunction)) {
      returnType = functionTable[currentFunction].returnType;
    }

    output += " " + compileExpr(ret->value.get(), returnType);
  }

  output += ";\n";
}

void Compiler::compileVarDecl(const VarDecl *decl) {
  if (decl->isGlobal)
    return;

  ValueType actualType = decl->type;

  if (actualType == ValueType::INFERRED && decl->hasValue) {
    actualType = inferExprType(decl->value.get());
  }

  // check if it's a lambda expression
  if (decl->hasValue) {
    if (auto *lambda = dynamic_cast<const LambdaExpr *>(decl->value.get())) {
      // use the variable name as the function name
      std::string funcName = decl->name;
      std::string savedOutput = output;
      output = "";

      compileLambdaExpr(lambda, funcName);

      nestedFunctionDecls += output;
      output = savedOutput;

      // add the variable to symbol table so it can be referenced
      symbolTable[decl->name] = {actualType, decl->isConst, false, true,
                                 decl->isOptional};
      return;
    }
    // check if it's a struct constructor
    else if (auto *structCons =
                 dynamic_cast<const StructConstructor *>(decl->value.get())) {
      std::string structName = structCons->structName;

      output += indent();
      if (decl->isConst)
        output += "const ";
      output += structName + " " + decl->name;
      // local variables can use compound literals
      output += " = " +
                compileExpr(decl->value.get(), ValueType::STRUCT, false) +
                ";\n";

      // store in symbol table with STRUCT type
      symbolTable[decl->name] = {ValueType::STRUCT, decl->isConst, false, true,
                                 decl->isOptional};
      return;
    }
  }

  std::string ctype = getCType(actualType);

  // normal variable declaration
  output += indent();
  if (decl->isConst)
    output += "const ";
  output += ctype + " " + decl->name;

  if (decl->hasValue) {
    output += " = " + compileExpr(decl->value.get(), actualType, false);
  }

  output += ";\n";

  symbolTable[decl->name] = {actualType, decl->isConst, false, true,
                             decl->isOptional};
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
    // compound assignment: x += 5 becomes x = x + 5
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
      // x **= y becomes x = pow(x, y)
      output = indent() + assign->name + " = pow(" + assign->name + ", " +
               compileExpr(assign->value.get(), var.type) + ");\n";
      return;
    case BinaryOp::FLOOR_DIVIDE:
      // x //= y becomes x = (double)floor(x / y)
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

std::string Compiler::compileFunctionCall(const FunctionCall *call) {
  // handle built-in functions
  if (call->name == "tostring") {
    if (call->arguments.size() != 1) {
      error("tostring() expects exactly 1 argument", call->line);
      return "";
    }

    std::string arg = compileExpr(call->arguments[0].get());
    ValueType argType = inferExprType(call->arguments[0].get());

    // generate appropriate tostring call based on argument type
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
    // print is handled separately in compilePrintStmt
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

  // regular user-defined functions OR lambda variables
  if (!checkFunction(call->name)) {
    // check if it's a variable that holds a lambda
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

  // check if this is a nested function
  bool isNestedFunction = !funcInfo.isGlobal;

  size_t paramCount = funcInfo.parameters.size();
  size_t argCount = call->arguments.size();

  size_t expectedArgs = isNestedFunction
                            ? (paramCount - currentFunctionParams.size())
                            : paramCount;

  size_t requiredParams = 0;

  if (isNestedFunction) {
    // for nested functions, only count their own params
    size_t ownParamStart = currentFunctionParams.size();
    for (size_t i = ownParamStart; i < paramCount; i++) {
      size_t optionalIdx = i - ownParamStart;
      if (optionalIdx < funcInfo.parameterOptionals.size() &&
          !funcInfo.parameterOptionals[optionalIdx]) {
        requiredParams++;
      }
    }
  } else {
    // for global functions, count all params
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

  // if nested function, first pass parent's parameters
  if (isNestedFunction) {
    for (size_t i = 0; i < currentFunctionParams.size(); i++) {
      const auto &param = currentFunctionParams[i];
      result += param.first;

      if (i < currentFunctionParams.size() - 1 || argCount > 0) {
        result += ", ";
      }
    }
  }

  // then compile the actual arguments provided by the caller
  for (size_t i = 0; i < argCount; i++) {
    // calculate the parameter index in the function's parameter list
    size_t paramIdx = isNestedFunction ? (currentFunctionParams.size() + i) : i;

    if (paramIdx < funcInfo.parameters.size()) {
      ValueType paramType = funcInfo.parameters[paramIdx].second;
      result += compileExpr(call->arguments[i].get(), paramType);

      if (i < argCount - 1) {
        result += ", ";
      }
    }
  }

  // handle optional parameters that weren't provided
  size_t providedArgs =
      isNestedFunction ? (currentFunctionParams.size() + argCount) : argCount;

  for (size_t i = providedArgs; i < paramCount; i++) {
    if (i > 0 || (isNestedFunction && currentFunctionParams.size() > 0) ||
        argCount > 0) {
      result += ", ";
    }

    ValueType paramType = funcInfo.parameters[i].second;

    if (paramType == ValueType::NUMBER) {
      result += "HL_NIL_NUMBER";
    } else if (paramType == ValueType::STRING) {
      result += "NULL";
    } else if (paramType == ValueType::BOOL) {
      result += "-1";
    } else {
      result += "HL_NIL_NUMBER";
    }
  }

  result += ")";
  return result;
}

void Compiler::compilePrintStmt(const PrintStmt *print) {
  if (print->arguments.empty()) {
    output += indent() + "hl_print_newline();\n";
    return;
  }

  output += indent();

  for (size_t i = 0; i < print->arguments.size(); i++) {
    const auto &arg = print->arguments[i];

    // check if argument is valid
    if (arg.isIdentifier) {
      if (!checkVariable(arg.identifier)) {
        error("Cannot print undefined variable '" + arg.identifier +
                  "'. Variables must be declared before use.",
              print->line);
        output = "";
        return;
      }
    } else if (arg.expression) {
      if (!validateExpr(arg.expression.get())) {
        error("Undefined variable in expression", print->line);
        output = "";
        return;
      }
    }
  }

  // print all arguments on one line
  for (size_t i = 0; i < print->arguments.size(); i++) {
    const auto &arg = print->arguments[i];

    if (arg.isIdentifier) {
      // get variable info
      auto &varInfo = symbolTable[arg.identifier];
      if (varInfo.isOptional) {
        // optional variable: check if nil and print accordingly
        output +=
            "if (hl_is_nil_" + typeToString(varInfo.type) + "(" +
            arg.identifier + ")) hl_print_no_newline(\"nil\"); else hl_print_" +
            typeToString(varInfo.type) + "_no_newline(" + arg.identifier + ");";
      } else {
        output += "hl_print_" + typeToString(varInfo.type) + "_no_newline(" +
                  arg.identifier + ");";
      }
    } else if (arg.expression) {
      std::string expr = compileExpr(arg.expression.get());
      ValueType type = inferExprType(arg.expression.get());

      bool isOptional = false;

      // only check for optional if it's a simple variable reference
      if (auto *varExpr = dynamic_cast<const VarExpr *>(arg.expression.get())) {
        if (symbolTable.count(varExpr->name)) {
          isOptional = symbolTable[varExpr->name].isOptional;
        }
      }
      // force unwrap should also check the inner variable
      else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(
                   arg.expression.get())) {
        if (auto *innerVar =
                dynamic_cast<const VarExpr *>(unwrap->operand.get())) {
          if (symbolTable.count(innerVar->name)) {
            isOptional = symbolTable[innerVar->name].isOptional;
          }
        }
      }

      if (isOptional) {
        // for optional expressions, print value or "nil"
        output += "if (hl_is_nil_" + typeToString(type) + "(" + expr +
                  ")) hl_print_no_newline(\"nil\"); else hl_print_" +
                  typeToString(type) + "_no_newline(" + expr + ");";
      } else {
        output +=
            "hl_print_" + typeToString(type) + "_no_newline(" + expr + ");";
      }
    }

    if (i < print->arguments.size() - 1) {
      output += " hl_print_tab(); ";
    }
  }

  output += " hl_print_newline();\n";
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
  default:
    return "unknown";
  }
}
std::string Compiler::getCType(ValueType type) {
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
    return "void*";
  default:
    return "void";
  }
}

void Compiler::pushScope() {
  nonNilVarStack.push_back(nonNilVars);
  nonNilVars.clear();
}

void Compiler::popScope() {
  if (!nonNilVarStack.empty()) {
    nonNilVars = nonNilVarStack.back();
    nonNilVarStack.pop_back();
  }
}

void Compiler::markNonNil(const std::string &varName) {
  nonNilVars.insert(varName);
}

bool Compiler::isProvenNonNil(const std::string &varName) {
  // check current scope first
  if (nonNilVars.count(varName) > 0) {
    return true;
  }

  // check parent scopes
  for (auto it = nonNilVarStack.rbegin(); it != nonNilVarStack.rend(); ++it) {
    if (it->count(varName) > 0) {
      return true;
    }
  }

  return false;
}

void Compiler::compileIfStmt(const IfStmt *ifStmt) {
  pushScope();

  // check if condition is a direct variable reference
  if (auto *varExpr = dynamic_cast<const VarExpr *>(ifStmt->condition.get())) {
    // this is "if y" sugar, check if variable is optional
    if (symbolTable.count(varExpr->name)) {
      auto &varInfo = symbolTable[varExpr->name];
      if (varInfo.isOptional) {
        // variable is optional, so "if y" means "if y != nil"
        // mark as non-nil in then block
        markNonNil(varExpr->name);
      }
    }
  }
  // check if condition is a negated direct variable reference
  else if (auto *unaryExpr =
               dynamic_cast<const UnaryExpr *>(ifStmt->condition.get())) {
    if (unaryExpr->op == UnaryOp::NOT) {
      if (auto *innerVar =
              dynamic_cast<const VarExpr *>(unaryExpr->operand.get())) {
        // this is "if not y" sugar, check if variable is optional
        if (symbolTable.count(innerVar->name)) {
          auto &varInfo = symbolTable[innerVar->name];
          if (varInfo.isOptional) {
            // "if not y" means "if y == nil"
            // don't mark as non-nil in then block
          }
        }
      }
    }
  }
  // check if condition is a nil check
  else if (auto *binExpr =
               dynamic_cast<const BinaryExpr *>(ifStmt->condition.get())) {
    if (binExpr->op == BinaryOp::NOT_EQUAL) {
      if (dynamic_cast<const VarExpr *>(binExpr->left.get()) &&
          dynamic_cast<const NilExpr *>(binExpr->right.get())) {
        // this is "x == nil", x is nil in then block
        // don't mark as non-nil
      }
    }
  }

  // generate the if condition
  output += indent() + "if (";

  // handle sugar for optional variables
  if (auto *varExpr = dynamic_cast<const VarExpr *>(ifStmt->condition.get())) {
    if (symbolTable.count(varExpr->name)) {
      auto &varInfo = symbolTable[varExpr->name];
      if (varInfo.isOptional) {
        // "if y" becomes "if (!hl_is_nil_type(y))" where type is
        // number/string/bool
        output += "!" + generateNilCheck(varExpr->name, varInfo.type);
      } else {
        // not optional, just use the variable directly
        output += varExpr->name;
      }
    } else {
      output += varExpr->name;
    }
  }
  // handle "if not y" sugar
  else if (auto *unaryExpr =
               dynamic_cast<const UnaryExpr *>(ifStmt->condition.get())) {
    if (unaryExpr->op == UnaryOp::NOT) {
      if (auto *innerVar =
              dynamic_cast<const VarExpr *>(unaryExpr->operand.get())) {
        if (symbolTable.count(innerVar->name)) {
          auto &varInfo = symbolTable[innerVar->name];
          if (varInfo.isOptional) {
            // "if not y" becomes "if (hl_is_nil_type(y))"
            output += generateNilCheck(innerVar->name, varInfo.type);
          } else {
            // not optional, "if (!y)"
            output += "!" + compileExpr(unaryExpr->operand.get());
          }
        } else {
          output += "!" + compileExpr(unaryExpr->operand.get());
        }
      } else {
        output += compileExpr(ifStmt->condition.get());
      }
    } else {
      output += compileExpr(ifStmt->condition.get());
    }
  } else {
    output += compileExpr(ifStmt->condition.get());
  }

  output += ") {\n";
  indentLevel++;

  // compile then block with nil tracking
  for (const auto &stmt : ifStmt->thenBlock) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }
  indentLevel--;

  popScope();
  pushScope();

  output += indent() + "}";

  if (!ifStmt->elseBlock.empty()) {
    output += " else {\n";
    indentLevel++;

    // handle "if y", y is nil in else block
    if (auto *varExpr =
            dynamic_cast<const VarExpr *>(ifStmt->condition.get())) {
      if (symbolTable.count(varExpr->name)) {
        auto &varInfo = symbolTable[varExpr->name];
        if (varInfo.isOptional) {
          // "if y" means y is nil in else block
          // don't mark as non-nil
        }
      }
    }
    // handle "if not y", y is non-nil in else block
    else if (auto *unaryExpr =
                 dynamic_cast<const UnaryExpr *>(ifStmt->condition.get())) {
      if (unaryExpr->op == UnaryOp::NOT) {
        if (auto *innerVar =
                dynamic_cast<const VarExpr *>(unaryExpr->operand.get())) {
          if (symbolTable.count(innerVar->name)) {
            auto &varInfo = symbolTable[innerVar->name];
            if (varInfo.isOptional) {
              // "if not y" means y is non-nil in else block
              markNonNil(innerVar->name);
            }
          }
        }
      }
    }
    // handle existing nil check patterns
    else if (auto *binExpr =
                 dynamic_cast<const BinaryExpr *>(ifStmt->condition.get())) {
      if (binExpr->op == BinaryOp::EQUAL) {
        // Remove the unused 'varExpr' variable by not declaring it
        if (dynamic_cast<const VarExpr *>(binExpr->left.get()) &&
            dynamic_cast<const NilExpr *>(binExpr->right.get())) {
          // this is "x == nil", x is non-nil in else block
        }
      } else if (binExpr->op == BinaryOp::NOT_EQUAL) {
        // Remove the unused 'varExpr' variable by not declaring it
        if (dynamic_cast<const VarExpr *>(binExpr->left.get()) &&
            dynamic_cast<const NilExpr *>(binExpr->right.get())) {
          // this is "x != nil", x is nil in else block
          // don't mark as non-nil
        }
      }
    }

    for (const auto &stmt : ifStmt->elseBlock) {
      std::string savedOutput = output;
      output = "";
      compileStatement(stmt.get());
      savedOutput += output;
      output = savedOutput;
    }
    indentLevel--;
    output += indent() + "}";
  }

  output += "\n";

  popScope();
}

std::string Compiler::generateNilCheck(const std::string &varName,
                                       ValueType type) {
  switch (type) {
  case ValueType::NUMBER:
    return "isnan(" + varName + ")";
  case ValueType::STRING:
    return "(" + varName + " == NULL)";
  case ValueType::BOOL:
    return "(" + varName + " == -1)";
  default:
    return "isnan(" + varName + ")";
  }
}

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
    } else {
      return "HL_NIL_NUMBER";
    }
  } else if (auto *lambda = dynamic_cast<const LambdaExpr *>(expr)) {
    std::string funcName = generateUniqueName("lambda");
    std::string savedOutput = output;
    output = "";
    compileLambdaExpr(lambda, funcName);
    std::string lambdaCode = output;
    output = savedOutput + lambdaCode;
    return funcName;
  } else if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    return var->name;
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    return compileFunctionCall(call);
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

    // other binary operators
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
      return "(!" + operand + ")";
    }
  } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
    // use initializer for globals, compound literal for locals
    if (forGlobalInit) {
      return compileStructInitializer(structCons);
    } else {
      return compileStructConstructor(structCons);
    }
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
    return compileFieldAccess(fieldAccess);
  }
  return "0.0";
}

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
  } else if (dynamic_cast<const LambdaExpr *>(expr)) {
    return ValueType::FUNCTION;
  } else if (dynamic_cast<const FunctionCall *>(expr)) {
    return ValueType::NUMBER;
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
    return ValueType::NUMBER;
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    if (un->op == UnaryOp::NOT) {
      return ValueType::BOOL;
    }
    return ValueType::NUMBER;
  } else if (dynamic_cast<const StructConstructor *>(expr)) {
    return ValueType::STRUCT;
  } else if (dynamic_cast<const FieldAccessExpr *>(expr)) {
    return ValueType::INFERRED;
  }
  return ValueType::INFERRED;
}

std::string Compiler::getCTypeWithOptional(ValueType type, bool isOptional) {
  std::string ctype = getCType(type);
  if (isOptional) {
    return ctype;
  }
  return ctype;
}

std::string Compiler::valueToString(
    const std::variant<int64_t, double, std::string, bool> &value) {
  if (auto *i = std::get_if<int64_t>(&value)) {
    return std::to_string(*i) + ".0";
  } else if (auto *d = std::get_if<double>(&value)) {
    return std::to_string(*d);
  } else if (auto *s = std::get_if<std::string>(&value)) {
    return "\"" + *s + "\"";
  } else if (auto *b = std::get_if<bool>(&value)) {
    return *b ? "1" : "0";
  }
  return "0.0";
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

bool Compiler::checkVariable(const std::string &name) {
  return symbolTable.count(name) > 0 && symbolTable[name].isDefined;
}

bool Compiler::checkFunction(const std::string &name) {
  return functionTable.count(name) > 0;
}

bool Compiler::validateExpr(const Expr *expr) {
  if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (!checkVariable(var->name)) {
      return false;
    }
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    if (!checkFunction(call->name)) {
      return false;
    }
    for (const auto &arg : call->arguments) {
      if (!validateExpr(arg.get())) {
        return false;
      }
    }
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (!validateExpr(bin->left.get()) || !validateExpr(bin->right.get())) {
      return false;
    }
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    if (!validateExpr(un->operand.get())) {
      return false;
    }
  } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    if (!validateExpr(unwrap->operand.get())) {
      return false;
    }
  }
  return true;
}

std::string Compiler::compileExprForConcat(const Expr *expr) {
  ValueType type = inferExprType(expr);
  std::string compiled = compileExpr(expr);

  if (type == ValueType::NUMBER) {
    return "hl_tostring_number(" + compiled + ")";
  } else if (type == ValueType::BOOL) {
    return "hl_tostring_bool(" + compiled + ")";
  } else if (type == ValueType::STRING) {
    return compiled;
  } else {
    return "hl_tostring_string(" + compiled + ")";
  }
}

void Compiler::compileLambdaExpr(const LambdaExpr *lambda,
                                 std::string &funcName) {
  // funcName should already be set by the caller if they want a specific name
  if (funcName.empty()) {
    funcName = generateUniqueName("func");
  }

  // save current function context
  std::string savedFunction = currentFunction;
  currentFunction = funcName;

  // save parent function parameters
  auto savedFunctionParams = currentFunctionParams;
  currentFunctionParams = lambda->parameters;
  auto savedSymbolTable = symbolTable;

  // add parameters to symbol table first
  for (size_t i = 0; i < lambda->parameters.size(); i++) {
    const auto &param = lambda->parameters[i];
    ValueType paramType = param.second;
    if (paramType == ValueType::INFERRED) {
      paramType = ValueType::NUMBER;
    }

    symbolTable[param.first] = {paramType, false, false, true, false};
  }

  // determine the actual return type
  ValueType actualReturnType = lambda->returnType;

  if (actualReturnType == ValueType::INFERRED) {
    // check function body for return statements
    for (const auto &stmt : lambda->body) {
      if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
        if (ret->value) {
          actualReturnType = inferExprType(ret->value.get());
          break;
        }
      }
    }

    // if still INFERRED, default to NUMBER
    if (actualReturnType == ValueType::INFERRED) {
      actualReturnType = ValueType::NUMBER;
    }
  }

  std::string returnType = getCType(actualReturnType);

  // generate function signature with the provided name
  output += "static " + returnType + " " + funcName + "(";

  // generate parameter list
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

  // save current indent level and set to 1 for function body
  int savedIndent = indentLevel;
  indentLevel = 1;

  // compile function body
  for (const auto &stmt : lambda->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  // check if function ends with return statement
  bool hasReturnAtEnd = false;
  if (!lambda->body.empty()) {
    hasReturnAtEnd =
        dynamic_cast<const ReturnStmt *>(lambda->body.back().get()) != nullptr;
  }

  // if no return statement at end, add default return based on return type
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

  // add to function table
  FunctionInfo funcInfo;
  funcInfo.name = funcName;
  funcInfo.parameters = lambda->parameters;
  funcInfo.parameterOptionals = lambda->parameterOptionals;
  funcInfo.returnType = actualReturnType;
  funcInfo.isGlobal = false;
  funcInfo.nestedFunctions = {};
  functionTable[funcName] = funcInfo;

  // restore symbol table and function context
  symbolTable = savedSymbolTable;
  currentFunction = savedFunction;
  currentFunctionParams = savedFunctionParams;
}

bool Compiler::isOptionalExpr(const Expr *expr) {
  if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (symbolTable.count(var->name)) {
      return symbolTable[var->name].isOptional;
    }
  } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    return isOptionalExpr(unwrap->operand.get());
  } else if (dynamic_cast<const FunctionCall *>(expr)) {
    return false;
  }
  return false;
}

std::string Compiler::generateUniqueName(const std::string &base) {
  static int counter = 0;
  return "__lambda_" + base + "_" + std::to_string(counter++);
}

} // namespace HolyLua