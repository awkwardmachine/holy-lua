#include "../../../include/compiler/compiler.h"

namespace HolyLua {

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