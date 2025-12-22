#pragma once
#include <iostream>
#include <string>
#include <vector>

namespace HolyLua {

class ErrorReporter {
public:
    ErrorReporter(const std::string &source);
    void reportError(const std::string &msg, int line);
    void showErrorContext(int line);
    bool hasErrors() const { return errorCount > 0; }
    int getErrorCount() const { return errorCount; }
    
private:
    std::string source;
    std::vector<std::string> sourceLines;
    int errorCount = 0;
    
    void initSourceLines();
};

} // namespace HolyLua