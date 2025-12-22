#include "../../include/compiler/compiler.h"
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

std::string Compiler::indent() { 
  return std::string(indentLevel * 4, ' '); 
}

std::string Compiler::compile(const Program &program) {
  std::string globalDecls = "";
  std::string mainBody = "";
  std::string functionDecls = "";
  std::string structDefinitions = "";
  std::string enumDefinitions = "";
  nestedFunctionDecls = "";
  bool hasErrors = false;
  bool hasMainFunction = false;

  // compile all enum declarations
  for (const auto &stmt : program.statements) {
    if (auto *enumDecl = dynamic_cast<EnumDecl *>(stmt.get())) {
      output = "";
      compileEnumDecl(enumDecl);
      enumDefinitions += output;
      output = "";
    }
  }

  // compile all struct declarations
  for (const auto &stmt : program.statements) {
    if (auto *structDecl = dynamic_cast<StructDecl *>(stmt.get())) {
      compileStructDecl(structDecl);
    }
  }

  // collect all struct definitions
  for (const auto &pair : structDefs) {
    structDefinitions += pair.second;
  }

  // collect class declarations
  for (const auto &stmt : program.statements) {
    if (auto *classDecl = dynamic_cast<ClassDecl *>(stmt.get())) {
      ClassInfo info;
      info.name = classDecl->name;
      classTable.insert({classDecl->name, std::move(info)});
    }
  }

  // collect function info
  for (const auto &stmt : program.statements) {
    if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
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

  // collect only global vars
  for (const auto &stmt : program.statements) {
    if (auto *decl = dynamic_cast<VarDecl *>(stmt.get())) {
      if (decl->isGlobal) {
        std::string varType;
        std::string structTypeName = "";
        ValueType actualType = decl->type;
        
        if (decl->type == ValueType::INFERRED) {
          if (decl->value) {
            actualType = inferExprType(decl->value.get());
            
            if (actualType == ValueType::STRUCT) {
              if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
                varType = classInst->className;
                structTypeName = classInst->className;
              } else {
                varType = "void*";
              }
            } else {
              varType = getCType(actualType);
            }
          } else {
            actualType = ValueType::NUMBER;
            varType = "double";
          }
        } else {
          actualType = decl->type;
          varType = getCType(actualType);
        }
        
        globalDecls += varType + " " + decl->name + ";\n";
        
        Variable var;
        var.type = actualType;
        var.isConst = decl->isConst;
        var.isDefined = false;
        var.isOptional = false;
        var.isFunction = false;
        var.isStruct = (actualType == ValueType::STRUCT);
        var.structTypeName = structTypeName;
        symbolTable[decl->name] = var;
      }
    }
  }

  // compile class declarations
  for (const auto &stmt : program.statements) {
    if (auto *classDecl = dynamic_cast<ClassDecl *>(stmt.get())) {
      output = "";
      compileClassDecl(classDecl);
      structDefinitions += output;
      output = "";
    }
  }

  // compile function definitions
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

  // generate output
  output = "#include \"api/holylua_api.h\"\n\n";
  output += enumDefinitions;
  output += structDefinitions;
  output += globalDecls + "\n";

  if (!nestedFunctionDecls.empty()) {
    output += nestedFunctionDecls + "\n";
  }

  output += functionDecls + "\n";

  if (hasMainFunction) {
    return output;
  }

  // generate main function
  output += "int main() {\n";
  indentLevel = 1;
  std::string savedOutput = output;
  output = "";

  // clear symbol table for main() scope
  auto mainSymbolTable = symbolTable;
  symbolTable.clear();

  for (const auto &stmt : program.statements) {
    if (dynamic_cast<EnumDecl *>(stmt.get())) {
      // skip
    } else if (dynamic_cast<StructDecl *>(stmt.get())) {
      // skip
    } else if (dynamic_cast<ClassDecl *>(stmt.get())) {
      // skip
    } else if (auto *func = dynamic_cast<FunctionDecl *>(stmt.get())) {
      // skip
    } else if (auto *decl = dynamic_cast<VarDecl *>(stmt.get())) {
      if (!decl->isGlobal) {
        // compile as a local variable declaration
        std::string varType;
        std::string structTypeName = "";
        ValueType actualType = decl->type;

        if (actualType == ValueType::STRUCT && decl->typeName == "struct" && decl->value) {
          // try to infer actual type from value
          if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
            varType = classInst->className;
            structTypeName = classInst->className;
            actualType = ValueType::STRUCT;
          } else if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
            varType = structCons->structName;
            structTypeName = structCons->structName;
            actualType = ValueType::STRUCT;
          } else {
            error("Cannot determine struct type for variable '" + decl->name + 
                  "'. Please specify the exact type or provide a value from which the type can be inferred.", 
                  decl->line);
            hasErrors = true;
            break;
          }
        }
        // determine the type from type annotation or value
        else if (actualType == ValueType::INFERRED) {
          if (decl->value) {
            actualType = inferExprType(decl->value.get());
            
            if (actualType == ValueType::STRUCT) {
              if (auto *classInst = dynamic_cast<const ClassInstantiation *>(decl->value.get())) {
                varType = classInst->className;
                structTypeName = classInst->className;
              } else if (auto *structCons = dynamic_cast<const StructConstructor *>(decl->value.get())) {
                varType = structCons->structName;
                structTypeName = structCons->structName;
              } else {
                varType = "void*";
              }
            } else {
              varType = getCType(actualType);
            }
          } else {
            actualType = ValueType::NUMBER;
            varType = "double";
          }
        } else {
          // use the declared type
          if (actualType == ValueType::STRUCT) {
            if (!decl->typeName.empty() && decl->typeName != "struct") {
              varType = decl->typeName;
              structTypeName = decl->typeName;
            } else {
              varType = "void*";
            }
          } else {
            varType = getCType(actualType);
          }
        }
        
        // generate the declaration
        output += indent();
        if (decl->isConst) output += "const ";
        output += varType + " " + decl->name;
        
        if (decl->value) {
          output += " = " + compileExpr(decl->value.get());
        }
        
        output += ";\n";
        
        // add to symbol table
        Variable var;
        var.type = actualType;
        var.isConst = decl->isConst;
        var.isDefined = true;
        var.isOptional = false;
        var.isFunction = false;
        var.isStruct = (actualType == ValueType::STRUCT);
        var.structTypeName = structTypeName;
        symbolTable[decl->name] = var;
        
        continue;
      } else {
        // global var
        if (decl->value) {
          output += indent() + decl->name + " = " + compileExpr(decl->value.get()) + ";\n";
          
          if (symbolTable.count(decl->name)) {
            symbolTable[decl->name].isDefined = true;
          }
        }
      }
    } else {
      // other statements
      std::string stmtOutput = output;
      output = "";
      compileStatement(stmt.get());
      stmtOutput += output;
      output = stmtOutput;
    }
  }

  if (hasErrors) {
    return "";
  }

  indentLevel = 0;
  output = savedOutput + output + "    return 0;\n}\n";

  // restore symbol table
  symbolTable = mainSymbolTable;

  return output;
}

} // namespace HolyLua