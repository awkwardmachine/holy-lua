#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compilePrintStmt(const PrintStmt *print) {
  if (print->arguments.empty()) {
    output += indent() + "hl_print_newline();\n";
    return;
  }

  output += indent();

  for (size_t i = 0; i < print->arguments.size(); i++) {
    const auto &arg = print->arguments[i];

    if (arg.isIdentifier) {
      if (!checkVariable(arg.identifier)) {
        error("Cannot print undefined variable '" + arg.identifier +
                  "'. Variables must be declared before use.",
              print->line);
        output = "";
        return;
      }
    } else if (arg.expression) {
      if (!validateExprForPrint(arg.expression.get())) {
        error("Undefined variable in expression", print->line);
        output = "";
        return;
      }
    }
  }

  for (size_t i = 0; i < print->arguments.size(); i++) {
    const auto &arg = print->arguments[i];

    if (i > 0) {
      output += " hl_print_tab(); ";
    }

    if (arg.isIdentifier) {
      auto &varInfo = symbolTable[arg.identifier];
      
      if (classTable.count(arg.identifier) > 0) {
        error("Cannot print class name directly: '" + arg.identifier + "'", print->line);
        output = "";
        return;
      }
      
      if (varInfo.isOptional) {
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
      
      if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(arg.expression.get())) {
        if (auto *varExpr = dynamic_cast<const VarExpr *>(fieldAccess->object.get())) {
          if (classTable.count(varExpr->name) > 0) {
            const auto &classInfo = classTable.at(varExpr->name);
            bool isStaticField = false;
            
            for (const auto &field : classInfo.fields) {
              if (field.name == fieldAccess->fieldName && field.isStatic) {
                isStaticField = true;
                type = field.type;
                break;
              }
            }
            
            if (isStaticField) {
              std::string fieldName = varExpr->name + "_" + fieldAccess->fieldName;
              output += "hl_print_" + typeToString(type) + "_no_newline(" + fieldName + ");";
              continue;
            }
          }
        }
      }

      if (auto *binExpr = dynamic_cast<const BinaryExpr *>(arg.expression.get())) {
        if (binExpr->op == BinaryOp::CONCAT) {
          output += "hl_print_string_no_newline(" + expr + ");";
          continue;
        }
      }

      if (auto *methodCall = dynamic_cast<const MethodCall *>(arg.expression.get())) {
        std::string className = "";
        if (auto *varExpr = dynamic_cast<const VarExpr *>(methodCall->object.get())) {
          if (classTable.count(varExpr->name)) {
            className = varExpr->name;
          }
        }
        
        if (!className.empty()) {
          const auto &classInfo = classTable.at(className);
          auto it = classInfo.methodInfo.find(methodCall->methodName);
          if (it != classInfo.methodInfo.end()) {
            type = it->second.first;
          }
        }
      }

      bool isOptional = false;
      if (auto *varExpr = dynamic_cast<const VarExpr *>(arg.expression.get())) {
        if (symbolTable.count(varExpr->name)) {
          isOptional = symbolTable[varExpr->name].isOptional;
        }
      } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(
                   arg.expression.get())) {
        if (auto *innerVar =
                dynamic_cast<const VarExpr *>(unwrap->operand.get())) {
          if (symbolTable.count(innerVar->name)) {
            isOptional = symbolTable[innerVar->name].isOptional;
          }
        }
      }

      if (isOptional) {
        output += "if (hl_is_nil_" + typeToString(type) + "(" + expr +
                  ")) hl_print_no_newline(\"nil\"); else hl_print_" +
                  typeToString(type) + "_no_newline(" + expr + ");";
      } else {
        output += "hl_print_" + typeToString(type) + "_no_newline(" + expr + ");";
      }
    }
  }

  output += " hl_print_newline();\n";
}

bool Compiler::validateExprForPrint(const Expr *expr) {
  if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (!checkVariable(var->name)) {
      return false;
    }
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    if (!checkFunction(call->name)) {
      if (call->name != "tostring" && call->name != "tonumber" && 
          call->name != "type") {
        return false;
      }
    }
    for (const auto &arg : call->arguments) {
      if (!validateExprForPrint(arg.get())) {
        return false;
      }
    }
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (!validateExprForPrint(bin->left.get()) || !validateExprForPrint(bin->right.get())) {
      return false;
    }
  } else if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
    return true;
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    if (!validateExprForPrint(un->operand.get())) {
      return false;
    }
  } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
    for (const auto &arg : structCons->positionalArgs) {
      if (!validateExprForPrint(arg.get())) {
        return false;
      }
    }
    for (const auto &namedArg : structCons->namedArgs) {
      if (!validateExprForPrint(namedArg.second.get())) {
        return false;
      }
    }
  } else if (dynamic_cast<const NilExpr *>(expr)) {
    return true;
  }
  return true;
}

} // namespace HolyLua