#include "../include/ast.h"
#include "../include/compiler/compiler.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/validation/type_checker.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

std::string getBaseName(const std::string &path) {
  size_t lastSlash = path.find_last_of("/\\");
  std::string filename =
      (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

  size_t lastDot = filename.find_last_of('.');
  if (lastDot != std::string::npos) {
    return filename.substr(0, lastDot);
  }
  return filename;
}

std::string getCurrentFolderName() {
  fs::path currentPath = fs::current_path();
  return currentPath.filename().string();
}

std::string getLibraryPath() {
  const char* holyLuaLib = std::getenv("HOLY_LUA_LIB");
  if (holyLuaLib) {
    std::string libPath = holyLuaLib;
    size_t lastSlash = libPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      return libPath.substr(0, lastSlash);
    }
  }
  return "./lib";
}

std::string getIncludePath() {
  const char* holyLuaInclude = std::getenv("HOLY_LUA_INCLUDE");
  if (holyLuaInclude) {
    return holyLuaInclude;
  }
  return "./include";
}

std::string readTomlValue(const std::string& content, const std::string& key) {
  size_t pos = content.find(key + " =");
  if (pos == std::string::npos) return "";
  
  size_t start = content.find('"', pos);
  if (start == std::string::npos) return "";
  start++;
  
  size_t end = content.find('"', start);
  if (end == std::string::npos) return "";
  
  return content.substr(start, end - start);
}

void initProject() {
  std::string projectName = "my-project";
  
  if (fs::exists("project.toml")) {
    std::cerr << "Error: project.toml already exists in this directory.\n";
    return;
  }
  
  fs::create_directories("src");
  
  std::ofstream tomlFile("project.toml");
  tomlFile << "[project]\n";
  tomlFile << "name = \"" << projectName << "\"\n";
  tomlFile << "version = \"0.0.1\"\n";
  tomlFile << "main = \"src/main.hlua\"\n";
  tomlFile.close();
  
  std::ofstream mainFile("src/main.hlua");
  mainFile << "function main()\n";
  mainFile << "    print(\"Hello, world!\")\n";
  mainFile << "end\n";
  mainFile.close();
  
  std::cout << "Initialized HolyLua project '" << projectName << "'\n";
  std::cout << "Created:\n";
  std::cout << "  - project.toml\n";
  std::cout << "  - src/main.hlua\n";
  std::cout << "\nRun 'holylua run' to execute your project.\n";
}

int compileFile(const std::string& inputFile, const std::string& outputName, 
                bool printAST, bool keepC, bool generateAsm) {
  std::ifstream file(inputFile);
  if (!file.is_open()) {
    std::cerr << "Could not open file: " << inputFile << "\n";
    return 1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();

  // lexical analysis
  HolyLua::Lexer lexer(source);
  auto tokens = lexer.scanTokens();

  if (lexer.hasErrors()) {
    std::cerr << "Lexical analysis failed due to errors.\n";
    return 1;
  }

  // parsing
  HolyLua::Parser parser(tokens, source);
  auto program = parser.parse();

  if (printAST) {
    std::cout << "\nAbstract Syntax Tree\n";
    HolyLua::ASTPrinter printer;
    printer.print(program);
  }

  HolyLua::TypeChecker typeChecker(source);
  if (!typeChecker.check(program)) {
    std::cerr << "Type checking failed due to errors.\n";
    return 1;
  }

  // code generation
  HolyLua::Compiler compiler(source);
  std::string cCode = compiler.compile(program);

  if (cCode.empty()) {
    std::cerr << "Compilation failed due to errors.\n";
    return 1;
  }

  // write C output
  std::string cFileName = outputName + ".c";
  std::ofstream outFile(cFileName);
  outFile << cCode;
  outFile.close();

  // get paths from environment variable
  std::string libPath = getLibraryPath();
  std::string includePath = getIncludePath();

  if (generateAsm) {
    std::string asmFileName = outputName + ".s";
    
    std::string gccCommand = "gcc -S -m64 -masm=intel "
                             "-fno-asynchronous-unwind-tables "
                             "-fno-ident "
                             "-fno-stack-protector "
                             "-O3 "
                             + cFileName + " -o " + asmFileName + 
                             " -I\"" + includePath + "\" -L\"" + libPath + "\" -lholylua -lm 2>&1";
    
    int result = system(gccCommand.c_str());
    
    if (result != 0) {
      std::cerr << "Failed to generate assembly with gcc.\n";
      if (!keepC) {
        std::remove(cFileName.c_str());
      }
      return 1;
    }
    
    if (!keepC) {
      std::remove(cFileName.c_str());
    }
    
  } else {
#ifdef _WIN32
    std::string exeName = outputName + ".exe";
#else
    std::string exeName = outputName;
#endif
    std::string gccCommand = "gcc \"" + cFileName + "\" -o \"" + exeName +
                           "\" -I\"" + includePath + "\" -L\"" + libPath + "\" -lholylua -lm 2>&1";

    int result = system(gccCommand.c_str());

    if (result != 0) {
      std::cerr << "Failed to compile C code with gcc.\n";
      if (!keepC) {
        std::remove(cFileName.c_str());
      }
      return 1;
    }

    if (!keepC) {
      std::remove(cFileName.c_str());
    }
  }

  return 0;
}

void runProject() {
  if (!fs::exists("project.toml")) {
    std::cerr << "Error: No project.toml found. Run 'holylua init' first.\n";
    return;
  }
  
  std::ifstream tomlFile("project.toml");
  std::stringstream buffer;
  buffer << tomlFile.rdbuf();
  std::string content = buffer.str();
  
  std::string mainFile = readTomlValue(content, "main");
  std::string projectName = readTomlValue(content, "name");
  std::string version = readTomlValue(content, "version");
  
  if (mainFile.empty()) {
    std::cerr << "Error: No 'main' file specified in project.toml\n";
    return;
  }
  
  if (!fs::exists(mainFile)) {
    std::cerr << "Error: Main file '" << mainFile << "' not found.\n";
    return;
  }
  
  std::cout << "Running project '" << projectName << "'...\n";
  
  fs::create_directories("build");

  std::string outputName = projectName + "-v" + version;
  std::string outputPath = "build" + std::string(1, fs::path::preferred_separator) + outputName;
  if (compileFile(mainFile, outputPath, false, false, false) == 0) {
#ifdef _WIN32
    system(("\"" + outputPath + ".exe\"").c_str());
#else
    system(("./" + outputPath).c_str());
#endif
  }
}

void buildProject() {
  if (!fs::exists("project.toml")) {
    std::cerr << "Error: No project.toml found. Run 'holylua init' first.\n";
    return;
  }
  
  std::ifstream tomlFile("project.toml");
  std::stringstream buffer;
  buffer << tomlFile.rdbuf();
  std::string content = buffer.str();
  
  std::string mainFile = readTomlValue(content, "main");
  std::string projectName = readTomlValue(content, "name");
  std::string version = readTomlValue(content, "version");
  
  if (mainFile.empty()) {
    std::cerr << "Error: No 'main' file specified in project.toml\n";
    return;
  }
  
  if (!fs::exists(mainFile)) {
    std::cerr << "Error: Main file '" << mainFile << "' not found.\n";
    return;
  }
  
  std::cout << "Building project '" << projectName << "'...\n";
  
  fs::create_directories("build");
  
  std::string outputName = projectName + "-v" + version;
  std::string outputPath = "build" + std::string(1, fs::path::preferred_separator) + outputName;
  if (compileFile(mainFile, outputPath, false, false, false) == 0) {
#ifdef _WIN32
    std::cout << "Build successful: " << outputPath << ".exe\n";
#else
    std::cout << "Build successful: " << outputPath << "\n";
#endif
  }
}

void printHelp() {
  std::cout << "HolyLua Compiler\n\n";
  std::cout << "Usage:\n";
  std::cout << "  holylua <file.hlua> [options]   Compile a single file\n";
  std::cout << "  holylua init                    Initialize a new project\n";
  std::cout << "  holylua run                     Run the current project\n";
  std::cout << "  holylua build                   Build the current project\n";
  std::cout << "  holylua help                    Show this help message\n";
  std::cout << "\nOptions:\n";
  std::cout << "  --ast         Print the AST (Abstract Syntax Tree)\n";
  std::cout << "  --keep-c      Keep the generated C file\n";
  std::cout << "  --asm         Generate assembly file instead of executable\n";
  std::cout << "  --o <name>    Specify output name\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printHelp();
    return 1;
  }

  std::string command = argv[1];
  
  std::transform(command.begin(), command.end(), command.begin(), ::tolower);
  
  if (command == "init") {
    initProject();
    return 0;
  } else if (command == "run") {
    runProject();
    return 0;
  } else if (command == "build") {
    buildProject();
    return 0;
  } else if (command == "help" || command == "--help" || command == "-h") {
    printHelp();
    return 0;
  }

  std::string inputFile = argv[1];
  bool printAST = false;
  bool keepC = false;
  bool generateAsm = false;
  std::string outputName = "";

  for (int i = 2; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--ast") {
      printAST = true;
    } else if (arg == "--keep-c") {
      keepC = true;
    } else if (arg == "--asm") {
      generateAsm = true;
    } else if (arg == "--o" && i + 1 < argc) {
      outputName = argv[i + 1];
      i++;
    }
  }

  if (outputName.empty()) {
    outputName = getBaseName(inputFile);
  }

  return compileFile(inputFile, outputName, printAST, keepC, generateAsm);
}