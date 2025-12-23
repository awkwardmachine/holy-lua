#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compileIfStmt(const IfStmt *ifStmt) {
  pushScope();

  if (auto *varExpr = dynamic_cast<const VarExpr *>(ifStmt->condition.get())) {
    if (symbolTable.count(varExpr->name)) {
      auto &varInfo = symbolTable[varExpr->name];
      if (varInfo.isOptional) {
        markNonNil(varExpr->name);
      }
    }
  }
  else if (auto *unaryExpr =
               dynamic_cast<const UnaryExpr *>(ifStmt->condition.get())) {
    if (unaryExpr->op == UnaryOp::NOT) {
      if (auto *innerVar =
              dynamic_cast<const VarExpr *>(unaryExpr->operand.get())) {
        if (symbolTable.count(innerVar->name)) {
          auto &varInfo = symbolTable[innerVar->name];
          if (varInfo.isOptional) {
            // don't mark as non-nil in then block
          }
        }
      }
    }
  }
  else if (auto *binExpr =
               dynamic_cast<const BinaryExpr *>(ifStmt->condition.get())) {
    if (binExpr->op == BinaryOp::NOT_EQUAL) {
      if (dynamic_cast<const VarExpr *>(binExpr->left.get()) &&
          dynamic_cast<const NilExpr *>(binExpr->right.get())) {
        // this is "x != nil", x is non-nil in then block
      }
    }
  }

  output += indent() + "if (";

  if (auto *varExpr = dynamic_cast<const VarExpr *>(ifStmt->condition.get())) {
    if (symbolTable.count(varExpr->name)) {
      auto &varInfo = symbolTable[varExpr->name];
      if (varInfo.isOptional) {
        output += "!" + generateNilCheck(varExpr->name, varInfo.type);
      } else {
        output += varExpr->name;
      }
    } else {
      output += varExpr->name;
    }
  }
  else if (auto *unaryExpr =
               dynamic_cast<const UnaryExpr *>(ifStmt->condition.get())) {
    if (unaryExpr->op == UnaryOp::NOT) {
      if (auto *innerVar =
              dynamic_cast<const VarExpr *>(unaryExpr->operand.get())) {
        if (symbolTable.count(innerVar->name)) {
          auto &varInfo = symbolTable[innerVar->name];
          if (varInfo.isOptional) {
            output += generateNilCheck(innerVar->name, varInfo.type);
          } else {
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

  for (const auto &stmt : ifStmt->thenBlock) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }
  indentLevel--;

  popScope();

  for (const auto &elseifBranch : ifStmt->elseifBranches) {
    pushScope();

    output += indent() + "} else if (";

    if (auto *varExpr = dynamic_cast<const VarExpr *>(elseifBranch.first.get())) {
      if (symbolTable.count(varExpr->name)) {
        auto &varInfo = symbolTable[varExpr->name];
        if (varInfo.isOptional) {
          output += "!" + generateNilCheck(varExpr->name, varInfo.type);
          markNonNil(varExpr->name);
        } else {
          output += varExpr->name;
        }
      } else {
        output += varExpr->name;
      }
    }
    else if (auto *unaryExpr =
                 dynamic_cast<const UnaryExpr *>(elseifBranch.first.get())) {
      if (unaryExpr->op == UnaryOp::NOT) {
        if (auto *innerVar =
                dynamic_cast<const VarExpr *>(unaryExpr->operand.get())) {
          if (symbolTable.count(innerVar->name)) {
            auto &varInfo = symbolTable[innerVar->name];
            if (varInfo.isOptional) {
              output += generateNilCheck(innerVar->name, varInfo.type);
            } else {
              output += "!" + compileExpr(unaryExpr->operand.get());
            }
          } else {
            output += "!" + compileExpr(unaryExpr->operand.get());
          }
        } else {
          output += compileExpr(elseifBranch.first.get());
        }
      } else {
        output += compileExpr(elseifBranch.first.get());
      }
    } else {
      output += compileExpr(elseifBranch.first.get());
    }

    output += ") {\n";
    indentLevel++;

    for (const auto &stmt : elseifBranch.second) {
      std::string savedOutput = output;
      output = "";
      compileStatement(stmt.get());
      savedOutput += output;
      output = savedOutput;
    }
    indentLevel--;

    popScope();
  }

  output += indent() + "}";

  if (!ifStmt->elseBlock.empty()) {
    pushScope();

    output += " else {\n";
    indentLevel++;

    if (auto *varExpr =
            dynamic_cast<const VarExpr *>(ifStmt->condition.get())) {
      if (symbolTable.count(varExpr->name)) {
        auto &varInfo = symbolTable[varExpr->name];
        if (varInfo.isOptional) {
          // "if y" means y is nil in else block
        }
      }
    }
    else if (auto *unaryExpr =
                 dynamic_cast<const UnaryExpr *>(ifStmt->condition.get())) {
      if (unaryExpr->op == UnaryOp::NOT) {
        if (auto *innerVar =
                dynamic_cast<const VarExpr *>(unaryExpr->operand.get())) {
          if (symbolTable.count(innerVar->name)) {
            auto &varInfo = symbolTable[innerVar->name];
            if (varInfo.isOptional) {
              markNonNil(innerVar->name);
            }
          }
        }
      }
    }
    else if (auto *binExpr =
                 dynamic_cast<const BinaryExpr *>(ifStmt->condition.get())) {
      if (binExpr->op == BinaryOp::EQUAL) {
        if (dynamic_cast<const VarExpr *>(binExpr->left.get()) &&
            dynamic_cast<const NilExpr *>(binExpr->right.get())) {
          // this is "x == nil", x is non-nil in else block
        }
      } else if (binExpr->op == BinaryOp::NOT_EQUAL) {
        if (dynamic_cast<const VarExpr *>(binExpr->left.get()) &&
            dynamic_cast<const NilExpr *>(binExpr->right.get())) {
          // this is "x != nil", x is nil in else block
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

    popScope();
  }

  output += "\n";
}

void Compiler::compileWhileStmt(const WhileStmt *whileStmt) {
  output +=
      indent() + "while (" + compileExpr(whileStmt->condition.get()) + ") {\n";

  pushScope();
  indentLevel++;

  for (const auto &stmt : whileStmt->body) {
    std::string savedOutput = output;
    output = "";
    compileStatement(stmt.get());
    savedOutput += output;
    output = savedOutput;
  }

  indentLevel--;
  popScope();
  output += indent() + "}\n";
}

void Compiler::compileForStmt(const ForStmt *forStmt) {
  pushScope();
  
  symbolTable[forStmt->varName] = {ValueType::NUMBER, false, true, false, false};

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

  symbolTable.erase(forStmt->varName);
  popScope();
}

void Compiler::compileRepeatStmt(const RepeatStmt *repeatStmt) {
  pushScope();

  output += indent() + "do {\n";

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

} // namespace HolyLua