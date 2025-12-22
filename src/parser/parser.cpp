#include "../../include/parser.h"
#include "../../include/token.h"
#include <iostream>
#include <sstream>

namespace HolyLua {

Parser::Parser(const std::vector<Token> &tokens, const std::string &source)
    : tokens(tokens), source(source) {
  initSourceLines();
}

void Parser::initSourceLines() {
  std::stringstream ss(source);
  std::string line;
  while (std::getline(ss, line)) {
    sourceLines.push_back(line);
  }
}

void Parser::error(const std::string &msg, int line) {
  std::cerr << "\033[1;31mError:\033[0m " << msg << "\n";
  showErrorContext(line);
}

void Parser::showErrorContext(int line) {
  if (line < 1 || line > (int)sourceLines.size())
    return;

  int lineIdx = line - 1;

  if (lineIdx > 0) {
    std::cerr << "  " << (lineIdx) << " | " << sourceLines[lineIdx - 1] << "\n";
  }

  std::cerr << "\033[1;33m> " << line << " | " << sourceLines[lineIdx]
            << "\033[0m\n";

  if (lineIdx < (int)sourceLines.size() - 1) {
    std::cerr << "  " << (lineIdx + 2) << " | " << sourceLines[lineIdx + 1]
              << "\n";
  }
  std::cerr << "\n";
}

Program Parser::parse() {
  Program program;

  while (!isAtEnd()) {
    skipNewlines();
    if (!isAtEnd() && peek().type != TokenType::END_OF_FILE) {
      auto stmt = statement();
      if (stmt) {
        program.statements.push_back(std::move(stmt));
      }
    }
  }

  return program;
}

} // namespace HolyLua
