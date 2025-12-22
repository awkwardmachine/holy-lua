#include "../../../include/compiler/compiler.h"

namespace HolyLua {

void Compiler::compileEnumDecl(const EnumDecl *decl) {
  enumTable[decl->name] = decl->values;
  
  // generate C enum
  output += "typedef enum {\n";
  
  for (size_t i = 0; i < decl->values.size(); i++) {
    output += "    " + decl->name + "_" + decl->values[i];
    if (i < decl->values.size() - 1) {
      output += ",";
    }
    output += "\n";
  }
  
  output += "} " + decl->name + ";\n\n";
}

std::string Compiler::compileEnumAccess(const EnumAccessExpr *expr) {
  if (enumTable.count(expr->enumName) == 0) {
    error("Unknown enum '" + expr->enumName + "'", expr->line);
    return "0";
  }
  
  // verify value exists in enum
  const auto &values = enumTable[expr->enumName];
  bool found = false;
  for (const auto &v : values) {
    if (v == expr->valueName) {
      found = true;
      break;
    }
  }
  
  if (!found) {
    error("Enum '" + expr->enumName + "' has no value '" + expr->valueName + "'", expr->line);
    return "0";
  }
  
  // generate enum access
  return expr->enumName + "_" + expr->valueName;
}

} // namespace HolyLua