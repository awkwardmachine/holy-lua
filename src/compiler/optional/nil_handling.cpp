#include "../../../include/compiler/compiler.h"

namespace HolyLua {

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

} // namespace HolyLua