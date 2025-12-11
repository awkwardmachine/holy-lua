#include "type_checker.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace HolyLua {

TypeChecker::TypeChecker(const std::string &source) : source(source) {
  initSourceLines();
}

void TypeChecker::initSourceLines() {
  std::stringstream ss(source);
  std::string line;
  while (std::getline(ss, line)) {
    sourceLines.push_back(line);
  }
}

void TypeChecker::error(const std::string &msg, int line) {
  std::cerr << "\033[1;31mType Error:\033[0m " << msg << "\n";
  showErrorContext(line);
  errorCount++;
}

void TypeChecker::showErrorContext(int line) {
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

void TypeChecker::initBuiltinFunctions() {
  // tostring: converts any value to string
  FunctionInfo tostringInfo;
  tostringInfo.name = "tostring";
  tostringInfo.parameters = {{"value", ValueType::INFERRED}};
  tostringInfo.parameterOptionals = {false};
  tostringInfo.returnType = ValueType::STRING;
  tostringInfo.isGlobal = true;
  tostringInfo.nestedFunctions = {};
  functionTable["tostring"] = tostringInfo;

  // print: prints any value
  FunctionInfo printInfo;
  printInfo.name = "print";
  printInfo.parameters = {{"value", ValueType::INFERRED}};
  printInfo.parameterOptionals = {false};
  printInfo.returnType = ValueType::INFERRED;
  printInfo.isGlobal = true;
  printInfo.nestedFunctions = {};
  functionTable["print"] = printInfo;

  // tonumber: converts string to number
  FunctionInfo tonumberInfo;
  tonumberInfo.name = "tonumber";
  tonumberInfo.parameters = {{"value", ValueType::STRING}};
  tonumberInfo.parameterOptionals = {false};
  tonumberInfo.returnType = ValueType::NUMBER;
  tonumberInfo.isGlobal = true;
  tonumberInfo.nestedFunctions = {};
  functionTable["tonumber"] = tonumberInfo;

  // type: returns the type of a value as string
  FunctionInfo typeInfo;
  typeInfo.name = "type";
  typeInfo.parameters = {{"value", ValueType::INFERRED}};
  typeInfo.parameterOptionals = {false};
  typeInfo.returnType = ValueType::STRING;
  typeInfo.isGlobal = true;
  typeInfo.nestedFunctions = {};
  functionTable["type"] = typeInfo;
}

bool TypeChecker::check(const Program &program) {
  initBuiltinFunctions();

  // collect struct declarations first
  if (!collectStructDeclarations(program)) {
    return false;
  }

  // first pass: collect global variable and constant declarations
  if (!collectGlobalDeclarations(program)) {
    return false;
  }

  // second pass: collect function signatures
  for (auto &stmt : program.statements) {
    if (!stmt)
      continue;
    if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
      if (!collectFunctionSignature(func)) {
        return false;
      }
    }
  }

  // third pass: infer types and validate functions
  for (auto &stmt : program.statements) {
    if (!stmt)
      continue;
    if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
      if (!inferAndValidateFunction(func)) {
        return false;
      }
    }
  }

  // fourth pass: check all statements
  for (const auto &stmt : program.statements) {
    if (!stmt)
      continue;
    // skip variable declarations as they were already checked
    if (dynamic_cast<const VarDecl *>(stmt.get())) {
      continue;
    }
    if (!checkStatement(stmt.get())) {
      return false;
    }
  }

  return errorCount == 0;
}

bool TypeChecker::collectStructDeclarations(const Program &program) {
  for (auto &stmt : program.statements) {
    if (!stmt)
      continue;

    if (auto *structDecl = dynamic_cast<StructDecl *>(stmt.get())) {
      // check if struct is already defined
      if (structTable.count(structDecl->name)) {
        error("Struct '" + structDecl->name + "' is already defined",
              structDecl->line);
        return false;
      }

      // add struct to table
      StructInfo info;
      info.name = structDecl->name;
      info.fields = structDecl->fields;

      // build field type map
      for (const auto &field : structDecl->fields) {
        info.fieldTypes[field.name] = {field.type, field.isOptional};
      }

      structTable[structDecl->name] = info;

      // also add to symbol table as a type (optional)
      symbolTable[structDecl->name] = {
          ValueType::STRUCT, false, true, false, false, true, structDecl->name};
    }
  }

  return true;
}

bool TypeChecker::collectFunctionSignature(FunctionDecl *func) {
  if (!func)
    return false;

  // only check for global function conflicts
  if (func->isGlobal && functionTable.count(func->name)) {
    error("Function '" + func->name + "' is already declared", func->line);
    return false;
  }

  // check that all parameters have explicit types
  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    if (param.second == ValueType::INFERRED) {
      error("Parameter '" + param.first + "' in function '" + func->name +
                "' must have an explicit type annotation (e.g., '" +
                param.first + ": number')",
            func->line);
      return false;
    }
  }

  FunctionInfo funcInfo;
  funcInfo.name = func->name;
  funcInfo.returnType = func->returnType;
  funcInfo.parameters = func->parameters;
  funcInfo.parameterOptionals = func->parameterOptionals;
  funcInfo.isGlobal = func->isGlobal;
  funcInfo.nestedFunctions = {};

  if (func->isGlobal) {
    functionTable[func->name] = funcInfo;
  }

  return true;
}

bool TypeChecker::inferAndValidateFunction(FunctionDecl *func) {
  if (!func)
    return false;

  // only process global functions in this pass
  if (!func->isGlobal) {
    return true;
  }

  // add parameters to symbol table
  auto savedSymbolTable = symbolTable;

  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    bool isOptional = func->parameterOptionals[i];

    symbolTable[param.first] = {param.second, false, true, isOptional,
                                false,        false, ""};
  }

  // analyze return types
  ReturnAnalysis returnAnalysis = analyzeReturnTypes(func->body);

  if (returnAnalysis.hasConflict) {
    std::string msg =
        "Function '" + func->name + "' has conflicting return types:\n";
    for (size_t i = 0; i < returnAnalysis.returnTypes.size(); i++) {
      msg += "  Line " + std::to_string(returnAnalysis.returnLines[i]) + ": " +
             typeToString(returnAnalysis.returnTypes[i]) + "\n";
    }
    msg += "Explicit return type required or logic must be unified";
    error(msg, func->line);
    symbolTable = savedSymbolTable;
    return false;
  }

  // if return type is inferred, set it
  if (func->returnType == ValueType::INFERRED) {
    if (returnAnalysis.inferredType != ValueType::INFERRED) {
      func->returnType = returnAnalysis.inferredType;
      functionTable[func->name].returnType = returnAnalysis.inferredType;
    } else {
      // no return statement found, default to void-like (NUMBER)
      func->returnType = ValueType::NUMBER;
      functionTable[func->name].returnType = ValueType::NUMBER;
    }
  } else {
    // validate explicit return type matches
    if (returnAnalysis.inferredType != ValueType::INFERRED &&
        !isCompatible(func->returnType, returnAnalysis.inferredType)) {
      error("Function '" + func->name + "' declared to return " +
                typeToString(func->returnType) + " but actually returns " +
                typeToString(returnAnalysis.inferredType),
            func->line);
      symbolTable = savedSymbolTable;
      return false;
    }
  }

  symbolTable = savedSymbolTable;

  return true;
}

bool TypeChecker::collectGlobalDeclarations(const Program &program) {
  for (auto &stmt : program.statements) {
    if (!stmt)
      continue;

    if (auto *decl = dynamic_cast<VarDecl *>(stmt.get())) {
      if (symbolTable.count(decl->name)) {
        error("Variable '" + decl->name + "' is already declared", decl->line);
        return false;
      }

      ValueType declaredType = decl->type;
      bool isOptional = decl->isOptional;
      bool isFunction = false;
      bool isStruct = false;
      std::string structTypeName = "";

      // check if type is a struct
      if (structTable.count(decl->typeName)) {
        isStruct = true;
        declaredType = ValueType::STRUCT;
        structTypeName = decl->typeName;
      }

      if (decl->hasValue && decl->value) {
        // check if it's a lambda
        if (dynamic_cast<const LambdaExpr *>(decl->value.get())) {
          isFunction = true;
          declaredType = ValueType::FUNCTION;

          if (decl->type != ValueType::INFERRED &&
              decl->type != ValueType::FUNCTION) {
            error("Cannot assign function to variable of type " +
                      typeToString(decl->type),
                  decl->line);
            return false;
          }
        }
        // check if it's a struct constructor
        else if (dynamic_cast<const StructConstructor *>(
                     decl->value.get())) {
          isStruct = true;
          declaredType = ValueType::STRUCT;

          // verify the struct exists by checking the actual struct constructor
          if (auto *structCons = dynamic_cast<const StructConstructor *>(
                  decl->value.get())) {
            structTypeName = structCons->structName;

            if (!structTable.count(structCons->structName)) {
              error("Struct '" + structCons->structName + "' is not defined",
                    decl->line);
              return false;
            }

            // validate that declared type matches
            if (decl->type != ValueType::INFERRED && !decl->typeName.empty()) {
              if (decl->typeName != structCons->structName) {
                error("Type mismatch: variable declared as '" + decl->typeName +
                          "' but initialized with '" + structCons->structName +
                          "'",
                      decl->line);
                return false;
              }
            }
          }
        }

        ValueType valueType = getExprType(decl->value.get());
        bool valueCanBeNil = canBeNil(decl->value.get());

        if (valueCanBeNil && !isOptional) {
          error("Cannot assign nil to non-optional variable '" + decl->name +
                    "'. Use explicit optional type (e.g., 'local " +
                    decl->name + ": number? = nil')",
                decl->line);
          return false;
        }

        if (declaredType != ValueType::INFERRED &&
            declaredType != ValueType::FUNCTION && !isStruct) {
          if (isOptional) {
            if (!valueCanBeNil && !isCompatible(declaredType, valueType)) {
              error("Type mismatch: variable '" + decl->name +
                        "' declared as " + typeToString(declaredType) +
                        "? but initialized with " + typeToString(valueType),
                    decl->line);
              return false;
            }
          } else {
            if (valueCanBeNil) {
              error("Cannot assign nil to non-optional variable '" +
                        decl->name + "'",
                    decl->line);
              return false;
            }
            if (!isCompatible(declaredType, valueType)) {
              error("Type mismatch: variable '" + decl->name +
                        "' declared as " + typeToString(declaredType) +
                        " but initialized with " + typeToString(valueType),
                    decl->line);
              return false;
            }
          }
        } else if (isStruct) {
          if (valueCanBeNil && !isOptional) {
            error("Cannot assign nil to non-optional struct variable '" +
                      decl->name + "'",
                  decl->line);
            return false;
          }
        } else {
          if (valueCanBeNil) {
            error(
                "Cannot infer type from nil. Use explicit optional type (e.g., "
                "'local " +
                    decl->name + ": number? = nil')",
                decl->line);
            return false;
          }
          declaredType = valueType;
        }
      } else {
        if (declaredType == ValueType::INFERRED && !isStruct) {
          error("Variable '" + decl->name +
                    "' must be initialized or have an explicit type",
                decl->line);
          return false;
        }

        if (!isOptional && !isStruct) {
          error(
              "Non-optional variable '" + decl->name +
                  "' must be initialized. " +
                  "Either provide a value or use optional type (e.g., 'local " +
                  decl->name + ": " + typeToString(declaredType) + "?')",
              decl->line);
          return false;
        }
      }

      // add to symbol table
      symbolTable[decl->name] = {declaredType,  decl->isConst, true,
                                 isOptional,    isFunction,    isStruct,
                                 structTypeName};
    }
  }

  return true;
}

bool TypeChecker::inferParameterTypes(FunctionDecl *func) {
  for (size_t i = 0; i < func->parameters.size(); i++) {
    auto &param = func->parameters[i];

    if (param.second != ValueType::INFERRED) {
      continue;
    }

    std::vector<UsageConstraint> constraints;
    auto inferredType =
        inferTypeFromUsage(param.first, func->body, constraints);

    // if parameter is used with any operator or in any type-constrained
    // context, require explicit type annotation for predictability
    if (!constraints.empty()) {
      std::string msg = "Cannot predict type of parameter '" + param.first +
                        "' in function '" + func->name + "'.\n";
      msg += "Parameters used in type-constrained contexts require explicit "
             "type annotations.\n";
      msg += "Usage contexts:\n";
      for (const auto &c : constraints) {
        msg += "  Line " + std::to_string(c.line) + ": " + c.context + "\n";
      }
      msg += "Add explicit type annotation, e.g., '" + param.first + ": " +
             typeToString(inferredType.value_or(ValueType::INFERRED)) + "'";
      error(msg, func->line);
      return false;
    }

    // if no constraints but also no usage, that's also an error
    if (constraints.empty()) {
      std::string msg = "Cannot infer type of unused parameter '" +
                        param.first + "' in function '" + func->name + "'.\n";
      msg += "Explicit type annotation required.";
      error(msg, func->line);
      return false;
    }
  }

  return true;
}

std::optional<ValueType> TypeChecker::inferTypeFromUsage(
    const std::string &paramName,
    const std::vector<std::unique_ptr<ASTNode>> &body,
    std::vector<UsageConstraint> &constraints) {

  // collect all usage constraints
  for (const auto &stmt : body) {
    if (!stmt)
      continue;
    collectUsageConstraints(paramName, stmt.get(), constraints);
  }

  if (constraints.empty()) {
    return std::nullopt;
  }

  // check if all constraints agree on the type
  ValueType inferredType = constraints[0].requiredType;
  for (const auto &constraint : constraints) {
    if (constraint.requiredType != inferredType) {
      return std::nullopt;
    }
  }

  return inferredType;
}

bool TypeChecker::hasAmbiguousUsage(
    const std::string &paramName,
    const std::vector<UsageConstraint> &constraints) {
  (void)paramName;
  
  if (constraints.size() < 2)
    return false;

  ValueType firstType = constraints[0].requiredType;
  for (size_t i = 1; i < constraints.size(); i++) {
    if (constraints[i].requiredType != firstType) {
      return true;
    }
  }
  return false;
}

void TypeChecker::collectUsageConstraints(
    const std::string &paramName, const ASTNode *node,
    std::vector<UsageConstraint> &constraints) {

  if (!node)
    return;

  if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
    if (ret->value) {
      collectExprConstraints(paramName, ret->value.get(), constraints);
    }
  } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
    collectExprConstraints(paramName, assign->value.get(), constraints);
  } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
    for (const auto &arg : print->arguments) {
      if (arg.expression) {
        collectExprConstraints(paramName, arg.expression.get(), constraints);
      }
    }
  } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(node)) {
    collectExprConstraints(paramName, ifStmt->condition.get(), constraints);
    for (const auto &stmt : ifStmt->thenBlock) {
      collectUsageConstraints(paramName, stmt.get(), constraints);
    }
    for (const auto &stmt : ifStmt->elseBlock) {
      collectUsageConstraints(paramName, stmt.get(), constraints);
    }
  } else if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
    if (decl->hasValue && decl->value) {
      collectExprConstraints(paramName, decl->value.get(), constraints);
    }
  } else if (auto *func = dynamic_cast<const FunctionDecl *>(node)) {
    for (const auto &stmt : func->body) {
      collectUsageConstraints(paramName, stmt.get(), constraints);
    }
  }
}

bool TypeChecker::isParameterInExpr(const std::string &paramName,
                                    const Expr *expr) {
  if (!expr)
    return false;

  if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    return var->name == paramName;
  } else if (auto *lambda = dynamic_cast<const LambdaExpr *>(expr)) {
    // check if parameter is used inside the lambda body
    for (const auto &stmt : lambda->body) {
      if (isParameterInNode(paramName, stmt.get())) {
        return true;
      }
    }
    return false;
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    return isParameterInExpr(paramName, bin->left.get()) ||
           isParameterInExpr(paramName, bin->right.get());
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    return isParameterInExpr(paramName, un->operand.get());
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    for (const auto &arg : call->arguments) {
      if (isParameterInExpr(paramName, arg.get()))
        return true;
    }
  } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    return isParameterInExpr(paramName, unwrap->operand.get());
  } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
    for (const auto &arg : structCons->positionalArgs) {
      if (isParameterInExpr(paramName, arg.get()))
        return true;
    }
    for (const auto &namedArg : structCons->namedArgs) {
      if (isParameterInExpr(paramName, namedArg.second.get()))
        return true;
    }
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
    return isParameterInExpr(paramName, fieldAccess->object.get());
  }

  return false;
}

bool TypeChecker::isParameterInNode(const std::string &paramName,
                                    const ASTNode *node) {
  if (!node)
    return false;

  if (auto *expr = dynamic_cast<const Expr *>(node)) {
    return isParameterInExpr(paramName, expr);
  } else if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
    if (ret->value) {
      return isParameterInExpr(paramName, ret->value.get());
    }
  } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
    return isParameterInExpr(paramName, assign->value.get());
  } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
    for (const auto &arg : print->arguments) {
      if (arg.expression &&
          isParameterInExpr(paramName, arg.expression.get())) {
        return true;
      }
    }
  } else if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
    if (decl->hasValue && decl->value) {
      return isParameterInExpr(paramName, decl->value.get());
    }
  }

  return false;
}

std::string TypeChecker::binaryOpToString(BinaryOp op) {
  switch (op) {
  case BinaryOp::ADD:
    return "+";
  case BinaryOp::SUBTRACT:
    return "-";
  case BinaryOp::MULTIPLY:
    return "*";
  case BinaryOp::DIVIDE:
    return "/";
  case BinaryOp::MODULO:
    return "%";
  case BinaryOp::CONCAT:
    return "..";
  case BinaryOp::EQUAL:
    return "==";
  case BinaryOp::NOT_EQUAL:
    return "!=";
  case BinaryOp::LESS:
    return "<";
  case BinaryOp::LESS_EQUAL:
    return "<=";
  case BinaryOp::GREATER:
    return ">";
  case BinaryOp::GREATER_EQUAL:
    return ">=";
  case BinaryOp::NIL_COALESCE:
    return "??";
  default:
    return "unknown";
  }
}

void TypeChecker::collectExprConstraints(
    const std::string &paramName, const Expr *expr,
    std::vector<UsageConstraint> &constraints, ValueType expectedType) {

  if (!expr)
    return;

  if (auto *lambda = dynamic_cast<const LambdaExpr *>(expr)) {
    // check if parameter is used inside the lambda body
    for (const auto &stmt : lambda->body) {
      if (isParameterInNode(paramName, stmt.get())) {
        constraints.emplace_back(ValueType::INFERRED, expr->line,
                                 "used inside lambda expression");
      }
    }
  }

  if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (var->name == paramName) {
      if (expectedType != ValueType::INFERRED) {
        constraints.emplace_back(expectedType, expr->line,
                                 "used in context expecting " +
                                     typeToString(expectedType));
      }
    }
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    // check if this is a usage of the parameter in a binary operation
    collectExprConstraints(paramName, bin->left.get(), constraints);
    collectExprConstraints(paramName, bin->right.get(), constraints);

    // also add operator-specific constraints
    ValueType requiredType;
    if (operatorRequiresType(bin->op, requiredType)) {
      // if the parameter is on either side of this operator, it has the
      // operator's type requirement
      if (isParameterInExpr(paramName, bin->left.get()) ||
          isParameterInExpr(paramName, bin->right.get())) {
        constraints.emplace_back(
            requiredType, expr->line,
            "used with operator '" + binaryOpToString(bin->op) +
                "' which requires " + typeToString(requiredType));
      }
    }
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    if (un->op == UnaryOp::NEGATE) {
      collectExprConstraints(paramName, un->operand.get(), constraints,
                             ValueType::NUMBER);
    } else {
      collectExprConstraints(paramName, un->operand.get(), constraints);
    }
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    // check arguments against function parameter types
    if (functionTable.count(call->name)) {
      const auto &funcInfo = functionTable[call->name];
      for (size_t i = 0;
           i < call->arguments.size() && i < funcInfo.parameters.size(); i++) {
        ValueType paramType = funcInfo.parameters[i].second;
        if (paramType != ValueType::INFERRED) {
          collectExprConstraints(paramName, call->arguments[i].get(),
                                 constraints, paramType);
        }
      }
    }
  } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    collectExprConstraints(paramName, unwrap->operand.get(), constraints);
  } else if (auto *structCons = dynamic_cast<const StructConstructor *>(expr)) {
    // For struct constructors, check if parameter is used in arguments
    for (const auto &arg : structCons->positionalArgs) {
      collectExprConstraints(paramName, arg.get(), constraints);
    }
    for (const auto &namedArg : structCons->namedArgs) {
      collectExprConstraints(paramName, namedArg.second.get(), constraints);
    }
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
    collectExprConstraints(paramName, fieldAccess->object.get(), constraints);
  }
}

bool TypeChecker::operatorRequiresType(BinaryOp op, ValueType &requiredType) {
  switch (op) {
  case BinaryOp::ADD:
  case BinaryOp::SUBTRACT:
  case BinaryOp::MULTIPLY:
  case BinaryOp::DIVIDE:
  case BinaryOp::MODULO:
    requiredType = ValueType::NUMBER;
    return true;

  case BinaryOp::CONCAT:
    requiredType = ValueType::STRING;
    return true;

  default:
    return false;
  }
}

ReturnAnalysis TypeChecker::analyzeReturnTypes(
    const std::vector<std::unique_ptr<ASTNode>> &body) {

  ReturnAnalysis analysis;

  for (const auto &stmt : body) {
    if (!stmt)
      continue;

    if (auto *ret = dynamic_cast<const ReturnStmt *>(stmt.get())) {
      if (ret->value) {
        ValueType retType = getExprType(ret->value.get());
        analysis.returnTypes.push_back(retType);
        analysis.returnLines.push_back(ret->line);
      }
    } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(stmt.get())) {
      // recursively check if/else blocks
      ReturnAnalysis thenAnalysis = analyzeReturnTypes(ifStmt->thenBlock);
      ReturnAnalysis elseAnalysis = analyzeReturnTypes(ifStmt->elseBlock);

      analysis.returnTypes.insert(analysis.returnTypes.end(),
                                  thenAnalysis.returnTypes.begin(),
                                  thenAnalysis.returnTypes.end());
      analysis.returnLines.insert(analysis.returnLines.end(),
                                  thenAnalysis.returnLines.begin(),
                                  thenAnalysis.returnLines.end());

      analysis.returnTypes.insert(analysis.returnTypes.end(),
                                  elseAnalysis.returnTypes.begin(),
                                  elseAnalysis.returnTypes.end());
      analysis.returnLines.insert(analysis.returnLines.end(),
                                  elseAnalysis.returnLines.begin(),
                                  elseAnalysis.returnLines.end());
    }
  }

  // check for conflicts
  if (analysis.returnTypes.size() > 1) {
    ValueType firstType = analysis.returnTypes[0];
    for (const auto &type : analysis.returnTypes) {
      if (type != firstType) {
        analysis.hasConflict = true;
        break;
      }
    }
  }

  // set inferred type
  if (!analysis.returnTypes.empty() && !analysis.hasConflict) {
    analysis.inferredType = analysis.returnTypes[0];
  }

  return analysis;
}

bool TypeChecker::checkStatement(const ASTNode *node) {
  if (!node)
    return true;

  if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
    return checkVarDecl(decl);
  } else if (auto *func = dynamic_cast<const FunctionDecl *>(node)) {
    return checkFunctionDecl(func);
  } else if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
    return checkReturnStmt(ret);
  } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
    return checkAssignment(assign);
  } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
    return checkPrintStmt(print);
  } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(node)) {
    return checkIfStmt(ifStmt);
  }
  return true;
}

bool TypeChecker::checkFunctionDecl(const FunctionDecl *func) {
  if (!func)
    return false;

  if (!func->isGlobal) {
    return checkNestedFunction(func);
  }

  if (!functionTable.count(func->name)) {
    error("Function '" + func->name + "' not found in function table",
          func->line);
    return false;
  }

  auto &funcInfo = functionTable[func->name];
  auto savedSymbolTable = symbolTable;
  auto savedNonNilVars = nonNilVars;
  std::string savedFunction = currentFunction;
  currentFunction = func->name;

  // add parameters to symbol table
  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    bool isOptional = func->parameterOptionals[i];
    symbolTable[param.first] = {param.second, false, true, isOptional,
                                false,        false, ""};
  }

  // first, collect nested function signatures
  for (const auto &stmt : func->body) {
    if (!stmt)
      continue;

    if (auto *nestedFunc = dynamic_cast<const FunctionDecl *>(stmt.get())) {
      // this is a nested function
      FunctionInfo nestedInfo;
      nestedInfo.name = nestedFunc->name;
      nestedInfo.returnType = nestedFunc->returnType;
      nestedInfo.parameters = nestedFunc->parameters;
      nestedInfo.parameterOptionals = nestedFunc->parameterOptionals;
      nestedInfo.isGlobal = false;
      nestedInfo.nestedFunctions = {};

      // add to parent function's nested functions
      funcInfo.nestedFunctions.push_back(nestedInfo);

      // also add to local symbol table so it can be called
      symbolTable[nestedFunc->name] = {
          ValueType::INFERRED, false, true, false, true, false, ""};
    }
  }

  // check function body
  bool hasErrors = false;
  for (const auto &stmt : func->body) {
    if (!stmt)
      continue;

    // skip nested function declarations as they were already collected
    if (dynamic_cast<const FunctionDecl *>(stmt.get())) {
      continue;
    }

    if (!checkStatement(stmt.get())) {
      hasErrors = true;
    }
  }

  symbolTable = savedSymbolTable;
  nonNilVars = savedNonNilVars;
  currentFunction = savedFunction;

  return !hasErrors;
}

bool TypeChecker::checkNestedFunction(const FunctionDecl *func) {
  if (!func)
    return false;

  // save current symbol table
  auto savedSymbolTable = symbolTable;
  auto savedNonNilVars = nonNilVars;
  std::string savedFunction = currentFunction;
  currentFunction = func->name;

  // add parameters to symbol table
  for (size_t i = 0; i < func->parameters.size(); i++) {
    const auto &param = func->parameters[i];
    bool isOptional = func->parameterOptionals[i];
    symbolTable[param.first] = {param.second, false, true, isOptional,
                                false,        false, ""};
  }

  // check function body
  bool hasErrors = false;
  for (const auto &stmt : func->body) {
    if (!stmt)
      continue;

    if (!checkStatement(stmt.get())) {
      hasErrors = true;
    }
  }

  // Restore symbol table
  symbolTable = savedSymbolTable;
  nonNilVars = savedNonNilVars;
  currentFunction = savedFunction;

  return !hasErrors;
}

bool TypeChecker::checkReturnStmt(const ReturnStmt *ret) {
  if (!ret)
    return true;

  if (ret->value) {
    getExprType(ret->value.get());
  }
  return true;
}

bool TypeChecker::checkVarDecl(const VarDecl *decl) {
  if (!decl)
    return false;

  if (decl->hasValue && decl->value) {
    // check if assigning a lambda
    if (auto *lambda = dynamic_cast<const LambdaExpr *>(decl->value.get())) {
      // for lambda assignments, we need to check the lambda body
      // save current context
      auto savedSymbolTable = symbolTable;
      std::string savedFunction = currentFunction;

      // create a unique name for this lambda
      currentFunction = "__lambda_" + decl->name;

      // add lambda parameters to symbol table
      for (size_t i = 0; i < lambda->parameters.size(); i++) {
        const auto &param = lambda->parameters[i];
        bool paramIsOptional = lambda->parameterOptionals[i];
        if (param.second == ValueType::INFERRED) {
          error("Lambda parameter '" + param.first +
                    "' must have explicit type",
                decl->line);
          return false;
        }
        symbolTable[param.first] = {param.second, false, true, paramIsOptional,
                                    false,        false, ""};
      }

      // check lambda body
      bool hasErrors = false;
      for (const auto &stmt : lambda->body) {
        if (!checkStatement(stmt.get())) {
          hasErrors = true;
        }
      }

      // restore context
      symbolTable = savedSymbolTable;
      currentFunction = savedFunction;

      if (hasErrors) {
        return false;
      }
    }
  }

  return true;
}

bool TypeChecker::checkAssignment(const Assignment *assign) {
  if (!assign)
    return false;

  if (!symbolTable.count(assign->name)) {
    error("Variable '" + assign->name + "' is not declared", assign->line);
    return false;
  }

  auto &varInfo = symbolTable[assign->name];

  if (varInfo.isConst) {
    error("Cannot assign to const variable '" + assign->name + "'",
          assign->line);
    return false;
  }

  ValueType valueType = getExprType(assign->value.get());
  bool valueCanBeNil = canBeNil(assign->value.get());

  // check if assigning a lambda
  if (auto *lambda = dynamic_cast<const LambdaExpr *>(assign->value.get())) {
    // for lambda assignments, check the lambda body
    // save current context
    auto savedSymbolTable = symbolTable;
    std::string savedFunction = currentFunction;

    // create a unique name for this lambda
    currentFunction = "__lambda_" + assign->name;

    // add lambda parameters to symbol table
    for (size_t i = 0; i < lambda->parameters.size(); i++) {
      const auto &param = lambda->parameters[i];
      bool paramIsOptional = lambda->parameterOptionals[i];
      if (param.second == ValueType::INFERRED) {
        error("Lambda parameter '" + param.first + "' must have explicit type",
              assign->line);
        return false;
      }
      symbolTable[param.first] = {param.second, false, true, paramIsOptional,
                                  false,        false, ""};
    }

    // check lambda body
    bool hasErrors = false;
    for (const auto &stmt : lambda->body) {
      if (!checkStatement(stmt.get())) {
        hasErrors = true;
      }
    }

    // restore context
    symbolTable = savedSymbolTable;
    currentFunction = savedFunction;

    if (hasErrors) {
      return false;
    }

    // update variable info to mark as function
    varInfo.type = ValueType::FUNCTION;
    varInfo.isFunction = true;
    valueType = ValueType::FUNCTION;
    valueCanBeNil = false;
  }

  if (assign->isCompound) {
    if (varInfo.isOptional && !isProvenNonNil(assign->name)) {
      error("Cannot use compound assignment on optional variable '" +
                assign->name +
                "' that might be nil. Use force unwrap (!) or check for nil "
                "first",
            assign->line);
      return false;
    }

    if (varInfo.type != ValueType::NUMBER || valueType != ValueType::NUMBER) {
      error("Compound assignment requires number types", assign->line);
      return false;
    }
  } else {
    if (varInfo.isOptional) {
      if (!valueCanBeNil && !isCompatible(varInfo.type, valueType)) {
        error("Type mismatch: cannot assign " + typeToString(valueType) +
                  " to variable '" + assign->name + "' of type " +
                  typeToString(varInfo.type) + "?",
              assign->line);
        return false;
      }
    } else {
      if (valueCanBeNil) {
        error("Cannot assign nil to non-optional variable '" + assign->name +
                  "'",
              assign->line);
        return false;
      }
      if (!isCompatible(varInfo.type, valueType)) {
        error("Type mismatch: cannot assign " + typeToString(valueType) +
                  " to variable '" + assign->name + "' of type " +
                  typeToString(varInfo.type),
              assign->line);
        return false;
      }
    }
  }

  return true;
}

bool TypeChecker::checkPrintStmt(const PrintStmt *print) {
  if (!print)
    return false;

  for (const auto &arg : print->arguments) {
    if (arg.isIdentifier) {
      if (!symbolTable.count(arg.identifier)) {
        error("Variable '" + arg.identifier + "' is not declared", print->line);
        return false;
      }
      auto &varInfo = symbolTable[arg.identifier];
      if (varInfo.isOptional && !isProvenNonNil(arg.identifier)) {
        error("Cannot print optional variable '" + arg.identifier +
                  "' that might be nil. Use force unwrap (!) or check for nil "
                  "first",
              print->line);
        return false;
      }
    } else if (arg.expression) {
      if (auto *varExpr = dynamic_cast<const VarExpr *>(arg.expression.get())) {
        if (symbolTable.count(varExpr->name)) {
          auto &varInfo = symbolTable[varExpr->name];
          if (varInfo.isOptional && !isProvenNonNil(varExpr->name)) {
            error("Cannot print optional variable '" + varExpr->name +
                      "' that might be nil. Use force unwrap (!) or check for "
                      "nil first",
                  print->line);
            return false;
          }
        }
      }
      getExprType(arg.expression.get());
    }
  }
  return true;
}

bool TypeChecker::checkIfStmt(const IfStmt *ifStmt) {
  if (!ifStmt)
    return false;

  getExprType(ifStmt->condition.get());

  std::unordered_set<std::string> savedNonNilVars = nonNilVars;

  if (auto *binExpr =
          dynamic_cast<const BinaryExpr *>(ifStmt->condition.get())) {
    if (binExpr->op == BinaryOp::NOT_EQUAL) {
      if (auto *varExpr = dynamic_cast<const VarExpr *>(binExpr->left.get())) {
        if (dynamic_cast<const NilExpr *>(binExpr->right.get())) {
          nonNilVars.insert(varExpr->name);
        }
      }
    }
  }

  for (const auto &stmt : ifStmt->thenBlock) {
    if (!stmt)
      continue;
    if (!checkStatement(stmt.get())) {
      return false;
    }
  }

  nonNilVars = savedNonNilVars;

  for (const auto &stmt : ifStmt->elseBlock) {
    if (!stmt)
      continue;
    if (!checkStatement(stmt.get())) {
      return false;
    }
  }

  nonNilVars = savedNonNilVars;

  return true;
}


ValueType TypeChecker::getExprType(const Expr *expr) {
  if (!expr)
    return ValueType::INFERRED;

  if (auto *lit = dynamic_cast<const LiteralExpr *>(expr)) {
    if (std::holds_alternative<int64_t>(lit->value)) {
      return ValueType::NUMBER;
    } else if (std::holds_alternative<double>(lit->value)) {
      return ValueType::NUMBER;
    } else if (std::holds_alternative<std::string>(lit->value)) {
      return ValueType::STRING;
    } else if (std::holds_alternative<bool>(lit->value)) {
      return ValueType::BOOL;
    }
  } else if (dynamic_cast<const NilExpr *>(expr)) {
    return ValueType::INFERRED;
  } else if (dynamic_cast<const LambdaExpr *>(expr)) {
    return ValueType::FUNCTION;
  } else if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (symbolTable.count(var->name)) {
      return symbolTable[var->name].type;
    } else {
      error("Variable '" + var->name + "' is not declared", expr->line);
      return ValueType::INFERRED;
    }
  } else if (auto *call = dynamic_cast<const FunctionCall *>(expr)) {
    // first: check if it's a variable in the symbol table
    if (symbolTable.count(call->name)) {
      auto &varInfo = symbolTable[call->name];

      // check if it's a function
      if (varInfo.isFunction || varInfo.type == ValueType::FUNCTION) {
        for (const auto &arg : call->arguments) {
          getExprType(arg.get());
        }
        return ValueType::INFERRED;
      }
    }

    if (call->name == "tostring") {
      if (call->arguments.size() != 1) {
        error("tostring() expects exactly 1 argument", expr->line);
        return ValueType::INFERRED;
      }
      getExprType(call->arguments[0].get());
      return ValueType::STRING;
    }

    if (call->name == "tonumber") {
      if (call->arguments.size() != 1) {
        error("tonumber() expects exactly 1 argument", expr->line);
        return ValueType::INFERRED;
      }
      ValueType argType = getExprType(call->arguments[0].get());
      if (argType != ValueType::STRING && argType != ValueType::INFERRED) {
        error("tonumber() expects a string argument", expr->line);
      }
      return ValueType::NUMBER;
    }

    if (call->name == "type") {
      if (call->arguments.size() != 1) {
        error("type() expects exactly 1 argument", expr->line);
        return ValueType::INFERRED;
      }
      getExprType(call->arguments[0].get());
      return ValueType::STRING;
    }

    if (call->name == "print") {
      for (const auto &arg : call->arguments) {
        getExprType(arg.get());
      }
      return ValueType::INFERRED;
    }

    // check if it's a global function
    if (functionTable.count(call->name)) {
      ValueType returnType = functionTable[call->name].returnType;
      if (returnType == ValueType::INFERRED) {
        return ValueType::NUMBER;
      }
      return returnType;
    }

    if (symbolTable.count(call->name)) {
      error("'" + call->name + "' is not a function", expr->line);
      return ValueType::INFERRED;
    } else {
      error("Function '" + call->name + "' is not declared", expr->line);
      return ValueType::INFERRED;
    }
  } else if (auto *unwrap = dynamic_cast<const ForceUnwrapExpr *>(expr)) {
    ValueType operandType = getExprType(unwrap->operand.get());

    // check if the operand is actually optional
    if (auto *varExpr = dynamic_cast<const VarExpr *>(unwrap->operand.get())) {
      if (symbolTable.count(varExpr->name)) {
        if (!symbolTable[varExpr->name].isOptional) {
          error("Cannot force unwrap non-optional variable '" + varExpr->name +
                    "'",
                expr->line);
        }
      }
    }

    return operandType;
  } else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr)) {
    if (bin->op == BinaryOp::NIL_COALESCE) {
      ValueType leftType = getExprType(bin->left.get());
      // right type is not used in the logic below
      getExprType(bin->right.get());

      // check if left is optional
      bool leftIsOptional = false;
      if (auto *varExpr = dynamic_cast<const VarExpr *>(bin->left.get())) {
        if (symbolTable.count(varExpr->name)) {
          leftIsOptional = symbolTable[varExpr->name].isOptional;
        }
      }

      if (!leftIsOptional) {
        error("Left side of ?? must be an optional type", expr->line);
      }

      // the result type is the non-optional version of left type
      return leftType;
    }

    ValueType leftType = getExprType(bin->left.get());
    getExprType(bin->right.get());

    // type checking for binary operations
    if (bin->op == BinaryOp::ADD || bin->op == BinaryOp::SUBTRACT ||
        bin->op == BinaryOp::MULTIPLY || bin->op == BinaryOp::DIVIDE ||
        bin->op == BinaryOp::MODULO) {
      if (leftType != ValueType::NUMBER && leftType != ValueType::INFERRED) {
        error("Left operand of arithmetic operator must be a number",
              expr->line);
      }
      ValueType rightType = getExprType(bin->right.get());
      if (rightType != ValueType::NUMBER && rightType != ValueType::INFERRED) {
        error("Right operand of arithmetic operator must be a number",
              expr->line);
      }
      return ValueType::NUMBER;
    }

    if (bin->op == BinaryOp::CONCAT) {
      if (leftType != ValueType::STRING && leftType != ValueType::INFERRED) {
        error("Left operand of concatenation must be a string", expr->line);
      }
      ValueType rightType = getExprType(bin->right.get());
      if (rightType != ValueType::STRING && rightType != ValueType::INFERRED) {
        error("Right operand of concatenation must be a string", expr->line);
      }
      return ValueType::STRING;
    }

    if (bin->op == BinaryOp::EQUAL || bin->op == BinaryOp::NOT_EQUAL ||
        bin->op == BinaryOp::LESS || bin->op == BinaryOp::LESS_EQUAL ||
        bin->op == BinaryOp::GREATER || bin->op == BinaryOp::GREATER_EQUAL) {
      ValueType rightType = getExprType(bin->right.get());
      if (leftType != ValueType::INFERRED && rightType != ValueType::INFERRED &&
          !isCompatible(leftType, rightType)) {
        error("Cannot compare " + typeToString(leftType) + " with " +
                  typeToString(rightType),
              expr->line);
      }
      return ValueType::BOOL;
    }

    return ValueType::INFERRED;
  } else if (auto *un = dynamic_cast<const UnaryExpr *>(expr)) {
    ValueType operandType = getExprType(un->operand.get());

    if (un->op == UnaryOp::NEGATE) {
      if (operandType != ValueType::NUMBER &&
          operandType != ValueType::INFERRED) {
        error("Cannot negate non-numeric value", expr->line);
      }
      return ValueType::NUMBER;
    } else if (un->op == UnaryOp::NOT) {
      // NOT operator works on bools
      return ValueType::BOOL;
    }

    return operandType;
  } else if (dynamic_cast<const StructConstructor *>(expr)) {
    return ValueType::STRUCT;
  } else if (auto *fieldAccess = dynamic_cast<const FieldAccessExpr *>(expr)) {
    ValueType objectType = getExprType(fieldAccess->object.get());

    if (objectType != ValueType::STRUCT) {
      error("Cannot access field '" + fieldAccess->fieldName +
                "' on non-struct type",
            expr->line);
      return ValueType::INFERRED;
    }

    // find the struct type
    std::string structName = "";
    if (auto *varExpr =
            dynamic_cast<const VarExpr *>(fieldAccess->object.get())) {
      if (symbolTable.count(varExpr->name)) {
        structName = symbolTable[varExpr->name].structTypeName;
      }
    }

    if (structName.empty() || !structTable.count(structName)) {
      error("Cannot determine struct type for field access", expr->line);
      return ValueType::INFERRED;
    }

    const auto &structInfo = structTable[structName];

    // find the field
    auto fieldIt = std::find_if(
        structInfo.fields.begin(), structInfo.fields.end(),
        [&](const StructField &f) { return f.name == fieldAccess->fieldName; });

    if (fieldIt == structInfo.fields.end()) {
      error("Struct '" + structName + "' has no field named '" +
                fieldAccess->fieldName + "'",
            expr->line);
      return ValueType::INFERRED;
    }

    return fieldIt->type;
  }

  return ValueType::INFERRED;
}

bool TypeChecker::checkExprType(const Expr *expr, ValueType expected,
                                int line) {
  ValueType actual = getExprType(expr);
  if (!isCompatible(expected, actual)) {
    error("Expected " + typeToString(expected) + " but got " +
              typeToString(actual),
          line);
    return false;
  }
  return true;
}

bool TypeChecker::isCompatible(ValueType expected, ValueType actual) {
  if (expected == ValueType::INFERRED || actual == ValueType::INFERRED) {
    return true;
  }

  if (expected == ValueType::STRUCT && actual == ValueType::STRUCT) {
    return true;
  }

  return expected == actual;
}

bool TypeChecker::canBeNil(const Expr *expr) {
  if (!expr)
    return false;

  if (dynamic_cast<const NilExpr *>(expr)) {
    return true;
  }
  if (auto *var = dynamic_cast<const VarExpr *>(expr)) {
    if (symbolTable.count(var->name)) {
      return symbolTable[var->name].isOptional;
    }
  }
  if (dynamic_cast<const StructConstructor *>(expr)) {
    return false;
  }
  if (dynamic_cast<const FunctionCall *>(expr)) {
    return false;
  }
  if (dynamic_cast<const LiteralExpr *>(expr)) {
    return false;
  }
  if (dynamic_cast<const LambdaExpr *>(expr)) {
    return false;
  }
  return false;
}

void TypeChecker::markNonNil(const std::string &varName) {
  nonNilVars.insert(varName);
}

bool TypeChecker::isProvenNonNil(const std::string &varName) {
  return nonNilVars.count(varName) > 0;
}

std::string TypeChecker::typeToString(ValueType type) {
  switch (type) {
  case ValueType::NUMBER:
    return "number";
  case ValueType::STRING:
    return "string";
  case ValueType::BOOL:
    return "bool";
  case ValueType::FUNCTION:
    return "function";
  case ValueType::STRUCT:
    return "struct";
  case ValueType::INFERRED:
    return "inferred";
  default:
    return "unknown";
  }
}

bool TypeChecker::isStructType(const std::string &typeName) {
  return structTable.count(typeName) > 0;
}

ValueType TypeChecker::resolveTypeName(const std::string &typeName) {
  if (typeName == "number")
    return ValueType::NUMBER;
  if (typeName == "string")
    return ValueType::STRING;
  if (typeName == "bool")
    return ValueType::BOOL;
  if (typeName == "function")
    return ValueType::FUNCTION;
  if (isStructType(typeName))
    return ValueType::STRUCT;
  return ValueType::INFERRED;
}

} // namespace HolyLua