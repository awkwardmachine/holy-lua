#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compileStructDecl(const StructDecl *decl) {
  StructInfo info;
  info.name = decl->name;
  info.fields = decl->fields;

  for (const auto &field : decl->fields) {
    info.fieldTypes[field.name] = {field.type, field.isOptional};
  }

  structTable[decl->name] = info;

  std::string structDef;
  structDef += "typedef struct {\n";
  for (const auto &field : info.fields) {
    std::string fieldType;
    if (field.type == ValueType::STRUCT) {
      if (!field.structTypeName.empty()) {
        fieldType = field.structTypeName;
      } else {
        error("Struct field '" + field.name + "' has unknown struct type", decl->line);
        fieldType = "void*";
      }
    } else if (field.type == ValueType::ENUM) {
      if (!field.structTypeName.empty()) {
        fieldType = field.structTypeName;
      } else {
        error("Struct field '" + field.name + "' has unknown enum type", decl->line);
        fieldType = "int";
      }
    } else {
      fieldType = getCType(field.type);
    }
    structDef += "    " + fieldType + " " + field.name + ";\n";
  }
  structDef += "} " + decl->name + ";\n\n";
  
  structDefs.push_back({decl->name, structDef});
}

std::string Compiler::compileStructConstructor(const StructConstructor *expr) {
  if (structTable.find(expr->structName) == structTable.end()) {
    error("Struct '" + expr->structName + "' not defined", expr->line);
    return "";
  }
  
  const auto &structInfo = structTable[expr->structName];
  std::string result = "(" + expr->structName + "){";

  if (expr->useDefaults) {
    bool first = true;
    for (const auto &field : structInfo.fields) {
      if (!first) result += ", ";
      first = false;

      if (field.hasDefault) {
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, int64_t>) {
                result += std::to_string(arg) + ".0";
              } else if constexpr (std::is_same_v<T, double>) {
                result += doubleToString(arg);
              } else if constexpr (std::is_same_v<T, std::string>) {
                result += "\"" + arg + "\"";
              } else if constexpr (std::is_same_v<T, bool>) {
                result += arg ? "1" : "0";
              } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                // nil handling for different types
                if (field.type == ValueType::ENUM) {
                  result += "-1";
                } else if (field.type == ValueType::NUMBER) {
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
        if (field.type == ValueType::ENUM) {
          result += "-1";
        } else if (field.type == ValueType::NUMBER) {
          result += "HL_NIL_NUMBER";
        } else if (field.type == ValueType::STRING) {
          result += "NULL";
        } else if (field.type == ValueType::BOOL) {
          result += "-1";
        } else {
          result += "0";
        }
      } else {
        if (field.type == ValueType::ENUM) {
          result += "0";
        } else if (field.type == ValueType::NUMBER) {
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
    if (expr->positionalArgs.size() > structInfo.fields.size()) {
      error("Too many arguments for struct '" + expr->structName + "'", expr->line);
      return "";
    }

    bool first = true;
    for (size_t i = 0; i < structInfo.fields.size(); i++) {
      if (!first) result += ", ";
      first = false;

      if (i < expr->positionalArgs.size()) {
        const auto &field = structInfo.fields[i];
        result += compileExpr(expr->positionalArgs[i].get(), field.type);
      } else {
        const auto &field = structInfo.fields[i];
        // handle default values for missing positional args
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
                  if (field.type == ValueType::ENUM) {
                    result += "-1";
                  } else if (field.type == ValueType::NUMBER) {
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
          if (field.type == ValueType::ENUM) {
            result += "-1";
          } else if (field.type == ValueType::NUMBER) {
            result += "HL_NIL_NUMBER";
          } else if (field.type == ValueType::STRING) {
            result += "NULL";
          } else if (field.type == ValueType::BOOL) {
            result += "-1";
          } else {
            result += "0";
          }
        } else {
          if (field.type == ValueType::ENUM) {
            result += "0";
          } else if (field.type == ValueType::NUMBER) {
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
      if (!first) result += ", ";
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
                if (field.type == ValueType::ENUM) {
                  result += "-1";
                } else if (field.type == ValueType::NUMBER) {
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
        if (field.type == ValueType::ENUM) {
          result += "-1";
        } else if (field.type == ValueType::NUMBER) {
          result += "HL_NIL_NUMBER";
        } else if (field.type == ValueType::STRING) {
          result += "NULL";
        } else if (field.type == ValueType::BOOL) {
          result += "-1";
        } else {
          result += "0";
        }
      } else {
        if (field.type == ValueType::ENUM) {
          result += "0";
        } else if (field.type == ValueType::NUMBER) {
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

std::string Compiler::compileStructInitializer(const StructConstructor *expr) {
  if (structTable.find(expr->structName) == structTable.end()) {
    error("Struct '" + expr->structName + "' not defined", expr->line);
    return "";
  }

  const auto &structInfo = structTable[expr->structName];

  std::string result = "{";

  if (expr->useDefaults) {
    bool first = true;
    for (const auto &field : structInfo.fields) {
      if (!first)
        result += ", ";
      first = false;

      if (field.hasDefault) {
        std::visit(
            [&](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, int64_t>) {
                result += std::to_string(arg) + ".0";
              } else if constexpr (std::is_same_v<T, double>) {
                result += doubleToString(arg);
              } else if constexpr (std::is_same_v<T, std::string>) {
                result += "\"" + arg + "\"";
              } else if constexpr (std::is_same_v<T, bool>) {
                result += arg ? "1" : "0";
              } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                // nil handling
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
  } else if (!expr->positionalArgs.empty()) {
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

} // namespace HolyLua