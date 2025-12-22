#include "../../../include/compiler/compiler.h"
#include <iostream>

namespace HolyLua {

void Compiler::error(const std::string &msg, int line) {
  std::cerr << "\033[1;31mError:\033[0m " << msg << "\n";
  showErrorContext(line);
}

void Compiler::showErrorContext(int line) {
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

} // namespace HolyLua