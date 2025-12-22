#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compileStatement(const ASTNode *node) {
  if (!node)
    return;

  if (auto *decl = dynamic_cast<const VarDecl *>(node)) {
    compileVarDecl(decl);
  } else if (auto *func = dynamic_cast<const FunctionDecl *>(node)) {
    compileFunctionDecl(func);
  } else if (auto *ret = dynamic_cast<const ReturnStmt *>(node)) {
    compileReturnStmt(ret);
  } else if (auto *assign = dynamic_cast<const Assignment *>(node)) {
    compileAssignment(assign);
  } else if (auto *fieldAssign = dynamic_cast<const FieldAssignment *>(node)) {
    compileFieldAssignment(fieldAssign);
  } else if (auto *print = dynamic_cast<const PrintStmt *>(node)) {
    compilePrintStmt(print);
  } else if (auto *ifStmt = dynamic_cast<const IfStmt *>(node)) {
    compileIfStmt(ifStmt);
  } else if (auto *call = dynamic_cast<const FunctionCall *>(node)) {
    std::string result = compileFunctionCall(call);
    if (!result.empty()) {
      output += indent() + result + ";\n";
    }
  } else if (auto *methodCall = dynamic_cast<const MethodCall *>(node)) {
    std::string result = compileMethodCall(methodCall);
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
  } else if (auto *classDecl = dynamic_cast<const ClassDecl *>(node)) {
    compileClassDecl(classDecl);
  } else if (auto *enumDecl = dynamic_cast<const EnumDecl *>(node)) {
    compileEnumDecl(enumDecl);
  }
}

void Compiler::compileInlineCStmt(const InlineCStmt *stmt) {
  output += indent() + stmt->cCode + "\n";
}

} // namespace HolyLua