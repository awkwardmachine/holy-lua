#include "ast.h"
#include <iostream>
#include <sstream>
#include <variant>

namespace HolyLua {

std::string valueTypeToString(ValueType type) {
  switch (type) {
  case ValueType::NUMBER:
    return "number";
  case ValueType::STRING:
    return "string";
  case ValueType::BOOL:
    return "bool";
  case ValueType::INFERRED:
    return "inferred";
  case ValueType::FUNCTION:
    return "function";
  case ValueType::STRUCT:
    return "struct";
  default:
    return "unknown";
  }
}

void ASTPrinter::print(const ASTNode *node) {
  if (!node)
    return;

  if (auto expr = dynamic_cast<const LiteralExpr *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const VarExpr *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const LambdaExpr *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const FunctionCall *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const BinaryExpr *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const UnaryExpr *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const ForceUnwrapExpr *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const NilExpr *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const StructConstructor *>(node)) {
    print(expr);
  } else if (auto expr = dynamic_cast<const FieldAccessExpr *>(node)) {
    print(expr);
  } else if (auto stmt = dynamic_cast<const VarDecl *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const FunctionDecl *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const ReturnStmt *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const Assignment *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const PrintStmt *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const IfStmt *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const InlineCStmt *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const WhileStmt *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const ForStmt *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const RepeatStmt *>(node)) {
    print(stmt);
  } else if (auto stmt = dynamic_cast<const StructDecl *>(node)) {
    print(stmt);
  }
}

void ASTPrinter::print(const RepeatStmt *stmt) {
  std::cout << getIndent() << "RepeatStmt (line: " << stmt->line << ")\n";
  indentLevel++;

  if (!stmt->body.empty()) {
    std::cout << getIndent() << "Body:\n";
    indentLevel++;
    for (const auto &node : stmt->body) {
      print(node.get());
    }
    indentLevel--;
  }

  std::cout << getIndent() << "Until Condition:\n";
  indentLevel++;
  print(stmt->condition.get());
  indentLevel--;
  indentLevel--;
}

void ASTPrinter::print(const ForStmt *stmt) {
  std::cout << getIndent() << "ForStmt (line: " << stmt->line << ")\n";
  indentLevel++;
  std::cout << getIndent() << "Variable: " << stmt->varName << "\n";
  std::cout << getIndent() << "Start:\n";
  indentLevel++;
  print(stmt->start.get());
  indentLevel--;

  std::cout << getIndent() << "End:\n";
  indentLevel++;
  print(stmt->end.get());
  indentLevel--;

  if (stmt->step) {
    std::cout << getIndent() << "Step:\n";
    indentLevel++;
    print(stmt->step.get());
    indentLevel--;
  }

  if (!stmt->body.empty()) {
    std::cout << getIndent() << "Body:\n";
    indentLevel++;
    for (const auto &node : stmt->body) {
      print(node.get());
    }
    indentLevel--;
  }
  indentLevel--;
}

void ASTPrinter::print(const InlineCStmt *stmt) {
  std::cout << getIndent() << "InlineCStmt";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  std::cout << ":\n";

  indentLevel++;
  std::stringstream ss(stmt->cCode);
  std::string line;
  while (std::getline(ss, line)) {
    std::cout << getIndent() << line << "\n";
  }
  indentLevel--;
}

void ASTPrinter::print(const Program &program) {
  std::cout << "=== Program AST ===\n";
  for (const auto &stmt : program.statements) {
    print(stmt.get());
  }
  std::cout << "===================\n";
}

void ASTPrinter::print(const LiteralExpr *expr) {
  std::cout << getIndent() << "LiteralExpr";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": ";

  std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {
          std::cout << arg << " (int)";
        } else if constexpr (std::is_same_v<T, double>) {
          std::cout << arg << " (double)";
        } else if constexpr (std::is_same_v<T, std::string>) {
          std::cout << "\"" << arg << "\" (string)";
        } else if constexpr (std::is_same_v<T, bool>) {
          std::cout << (arg ? "true" : "false") << " (bool)";
        }
      },
      expr->value);
  std::cout << "\n";
}

void ASTPrinter::print(const VarExpr *expr) {
  std::cout << getIndent() << "VarExpr";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": " << expr->name << "\n";
}

void ASTPrinter::print(const FunctionCall *expr) {
  std::cout << getIndent() << "FunctionCall";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": " << expr->name << "(\n";

  indentLevel++;
  for (const auto &arg : expr->arguments) {
    print(arg.get());
  }
  indentLevel--;

  std::cout << getIndent() << ")\n";
}

void ASTPrinter::printBinaryOp(BinaryOp op) {
  switch (op) {
  case BinaryOp::ADD:
    std::cout << "+";
    break;
  case BinaryOp::SUBTRACT:
    std::cout << "-";
    break;
  case BinaryOp::MULTIPLY:
    std::cout << "*";
    break;
  case BinaryOp::DIVIDE:
    std::cout << "/";
    break;
  case BinaryOp::FLOOR_DIVIDE:
    std::cout << "//";
    break;
  case BinaryOp::POWER:
    std::cout << "^";
    break;
  case BinaryOp::MODULO:
    std::cout << "%";
    break;
  case BinaryOp::EQUAL:
    std::cout << "==";
    break;
  case BinaryOp::NOT_EQUAL:
    std::cout << "!=";
    break;
  case BinaryOp::LESS:
    std::cout << "<";
    break;
  case BinaryOp::LESS_EQUAL:
    std::cout << "<=";
    break;
  case BinaryOp::GREATER:
    std::cout << ">";
    break;
  case BinaryOp::GREATER_EQUAL:
    std::cout << ">=";
    break;
  case BinaryOp::NIL_COALESCE:
    std::cout << "??";
    break;
  case BinaryOp::CONCAT:
    std::cout << "..";
    break;
  case BinaryOp::AND:
    std::cout << "&&";
    break;
  case BinaryOp::OR:
    std::cout << "||";
    break;
  }
}


void ASTPrinter::print(const BinaryExpr *expr) {
  std::cout << getIndent() << "BinaryExpr";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": (\n";

  indentLevel++;
  std::cout << getIndent() << "left: ";
  print(expr->left.get());

  std::cout << getIndent() << "op: ";
  printBinaryOp(expr->op);
  std::cout << "\n";

  std::cout << getIndent() << "right: ";
  print(expr->right.get());
  indentLevel--;

  std::cout << getIndent() << ")\n";
}

void ASTPrinter::printUnaryOp(UnaryOp op) {
  switch (op) {
  case UnaryOp::NEGATE:
    std::cout << "-";
    break;
  case UnaryOp::NOT:
    std::cout << "!";
    break;
  }
}

void ASTPrinter::print(const UnaryExpr *expr) {
  std::cout << getIndent() << "UnaryExpr";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": ";
  printUnaryOp(expr->op);
  std::cout << " (\n";

  indentLevel++;
  std::cout << getIndent() << "operand: ";
  print(expr->operand.get());
  indentLevel--;

  std::cout << getIndent() << ")\n";
}

void ASTPrinter::print(const ForceUnwrapExpr *expr) {
  std::cout << getIndent() << "ForceUnwrapExpr";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": (\n";

  indentLevel++;
  std::cout << getIndent() << "operand: ";
  print(expr->operand.get());
  indentLevel--;

  std::cout << getIndent() << ") !\n";
}

void ASTPrinter::print(const NilExpr *expr) {
  std::cout << getIndent() << "NilExpr";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": nil\n";
}

void ASTPrinter::print(const VarDecl *stmt) {
  std::cout << getIndent() << "VarDecl";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  std::cout << ": ";
  if (stmt->isGlobal)
    std::cout << "global ";
  if (stmt->isConst)
    std::cout << "const ";
  std::cout << stmt->name << ": ";
  std::cout << valueTypeToString(stmt->type);
  if (stmt->isOptional)
    std::cout << "?";

  if (stmt->hasValue) {
    std::cout << " =\n";
    indentLevel++;
    print(stmt->value.get());
    indentLevel--;
  } else {
    std::cout << "\n";
  }
}

void ASTPrinter::print(const FunctionDecl *stmt) {
  std::cout << getIndent() << "FunctionDecl";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  std::cout << ": ";
  if (stmt->isGlobal)
    std::cout << "global ";
  std::cout << stmt->name << "(";

  for (size_t i = 0; i < stmt->parameters.size(); i++) {
    const auto &param = stmt->parameters[i];
    std::cout << param.first << ": " << valueTypeToString(param.second);
    if (i < stmt->parameters.size() - 1)
      std::cout << ", ";
  }
  std::cout << ") -> " << valueTypeToString(stmt->returnType) << "\n";

  std::cout << getIndent() << "{\n";
  indentLevel++;
  for (const auto &bodyStmt : stmt->body) {
    print(bodyStmt.get());
  }
  indentLevel--;
  std::cout << getIndent() << "}\n";
}

void ASTPrinter::print(const ReturnStmt *stmt) {
  std::cout << getIndent() << "ReturnStmt";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  if (stmt->value) {
    std::cout << ":\n";
    indentLevel++;
    print(stmt->value.get());
    indentLevel--;
  } else {
    std::cout << " (void)\n";
  }
}

void ASTPrinter::print(const Assignment *stmt) {
  std::cout << getIndent() << "Assignment";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  std::cout << ": " << stmt->name;
  if (stmt->isCompound) {
    std::cout << " ";
    printBinaryOp(stmt->compoundOp);
  }
  std::cout << " =\n";

  indentLevel++;
  print(stmt->value.get());
  indentLevel--;
}

void ASTPrinter::print(const PrintStmt *stmt) {
  std::cout << getIndent() << "PrintStmt";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  std::cout << ": print(";

  for (size_t i = 0; i < stmt->arguments.size(); i++) {
    const auto &arg = stmt->arguments[i];
    if (arg.isIdentifier) {
      std::cout << arg.identifier;
    } else {
      std::cout << "\n";
      indentLevel++;
      std::cout << getIndent() << "expr: ";
      print(arg.expression.get());
      indentLevel--;
    }
    if (i < stmt->arguments.size() - 1)
      std::cout << ", ";
  }
  std::cout << ")\n";
}

void ASTPrinter::print(const IfStmt *stmt) {
  std::cout << getIndent() << "IfStmt";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  std::cout << ":\n";

  indentLevel++;
  std::cout << getIndent() << "condition:\n";
  indentLevel++;
  print(stmt->condition.get());
  indentLevel--;

  std::cout << getIndent() << "then:\n";
  indentLevel++;
  for (const auto &thenStmt : stmt->thenBlock) {
    print(thenStmt.get());
  }
  indentLevel--;

  if (!stmt->elseBlock.empty()) {
    std::cout << getIndent() << "else:\n";
    indentLevel++;
    for (const auto &elseStmt : stmt->elseBlock) {
      print(elseStmt.get());
    }
    indentLevel--;
  }
  indentLevel--;
}

void ASTPrinter::print(const WhileStmt *stmt) {
  std::cout << getIndent() << "WhileStmt (line: " << stmt->line << ")\n";
  indentLevel++;
  std::cout << getIndent() << "Condition:\n";
  indentLevel++;
  print(stmt->condition.get());
  indentLevel--;

  if (!stmt->body.empty()) {
    std::cout << getIndent() << "Body:\n";
    indentLevel++;
    for (const auto &node : stmt->body) {
      print(node.get());
    }
    indentLevel--;
  }
  indentLevel--;
}

void ASTPrinter::print(const LambdaExpr *expr) {
  std::cout << getIndent() << "LambdaExpr (line: " << expr->line << ")\n";
  indentLevel++;

  if (!expr->parameters.empty()) {
    std::cout << getIndent() << "Parameters:\n";
    indentLevel++;
    for (size_t i = 0; i < expr->parameters.size(); i++) {
      std::cout << getIndent() << expr->parameters[i].first;
      if (expr->parameters[i].second != ValueType::INFERRED) {
        std::cout << ": " << valueTypeToString(expr->parameters[i].second);
      }
      if (i < expr->parameterOptionals.size() && expr->parameterOptionals[i]) {
        std::cout << "?";
      }
      std::cout << "\n";
    }
    indentLevel--;
  }

  if (expr->returnType != ValueType::INFERRED) {
    std::cout << getIndent()
              << "Return type: " << valueTypeToString(expr->returnType) << "\n";
  }

  if (!expr->body.empty()) {
    std::cout << getIndent() << "Body:\n";
    indentLevel++;
    for (const auto &node : expr->body) {
      print(node.get());
    }
    indentLevel--;
  }
  indentLevel--;
}

void ASTPrinter::print(const StructDecl *stmt) {
  std::cout << getIndent() << "StructDecl";
  if (stmt->line)
    std::cout << " [line:" << stmt->line << "]";
  std::cout << ": " << stmt->name << " {\n";

  indentLevel++;
  for (const auto &field : stmt->fields) {
    std::cout << getIndent() << field.name << ": "
              << valueTypeToString(field.type);
    if (field.isOptional)
      std::cout << "?";
    if (field.hasDefault) {
      std::cout << " = ";
      std::visit(
          [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int64_t>) {
              std::cout << arg << " (int)";
            } else if constexpr (std::is_same_v<T, double>) {
              std::cout << arg << " (double)";
            } else if constexpr (std::is_same_v<T, std::string>) {
              std::cout << "\"" << arg << "\" (string)";
            } else if constexpr (std::is_same_v<T, bool>) {
              std::cout << (arg ? "true" : "false") << " (bool)";
            } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
              std::cout << "nil";
            }
          },
          field.defaultValue);
    }
    std::cout << "\n";
  }
  indentLevel--;
  std::cout << getIndent() << "}\n";
}

void ASTPrinter::print(const StructConstructor *expr) {
  std::cout << getIndent() << "StructConstructor";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": " << expr->structName << " {\n";

  indentLevel++;
  if (!expr->positionalArgs.empty()) {
    std::cout << getIndent() << "Positional Args:\n";
    indentLevel++;
    for (const auto &arg : expr->positionalArgs) {
      print(arg.get());
    }
    indentLevel--;
  }

  if (!expr->namedArgs.empty()) {
    std::cout << getIndent() << "Named Args:\n";
    indentLevel++;
    for (const auto &arg : expr->namedArgs) {
      std::cout << getIndent() << arg.first << ": ";
      print(arg.second.get());
    }
    indentLevel--;
  }

  if (expr->useDefaults) {
    std::cout << getIndent() << "Use Defaults: true\n";
  }
  indentLevel--;
  std::cout << getIndent() << "}\n";
}

void ASTPrinter::print(const FieldAccessExpr *expr) {
  std::cout << getIndent() << "FieldAccessExpr";
  if (expr->line)
    std::cout << " [line:" << expr->line << "]";
  std::cout << ": (\n";

  indentLevel++;
  std::cout << getIndent() << "object: ";
  print(expr->object.get());
  std::cout << getIndent() << "field: " << expr->fieldName << "\n";
  indentLevel--;

  std::cout << getIndent() << ")\n";
}

} // namespace HolyLua