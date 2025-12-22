#include "../../include/utils/error_reporter.h"
#include <sstream>

namespace HolyLua {

ErrorReporter::ErrorReporter(const std::string &source) : source(source), errorCount(0) {
    initSourceLines();
}

void ErrorReporter::initSourceLines() {
    std::stringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
        sourceLines.push_back(line);
    }
}

void ErrorReporter::reportError(const std::string &msg, int line) {
    std::cerr << "\033[1;31mType Error:\033[0m " << msg << "\n";
    showErrorContext(line);
    errorCount++;
}

void ErrorReporter::showErrorContext(int line) {
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