#include "../include/ast.h"
#include "../include/compiler/compiler.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/validation/type_checker.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

std::string getBaseName(const std::string &path) {
  // find last slash or backslash
  size_t lastSlash = path.find_last_of("/\\");
  std::string filename =
      (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

  // remove extension
  size_t lastDot = filename.find_last_of('.');
  if (lastDot != std::string::npos) {
    return filename.substr(0, lastDot);
  }
  return filename;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: holylua <file.hlua> [options]\n";
    std::cerr << "Options:\n";
    std::cerr << "  --ast         Print the AST (Abstract Syntax Tree)\n";
    std::cerr << "  --keep-c      Keep the generated C file\n";
    std::cerr << "  --asm         Generate assembly file instead of executable\n";
    std::cerr << "  --o <name>    Specify output name\n";
    return 1;
  }

  std::string inputFile = argv[1];
  bool printAST = false;
  bool keepC = false;
  bool generateAsm = false;
  std::string outputName = "";

  // parse flags
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

  // if no output name specified, use input filename without extension
  if (outputName.empty()) {
    outputName = getBaseName(inputFile);
  }

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

  // check for lexer errors
  if (lexer.hasErrors()) {
    std::cerr << "Lexical analysis failed due to errors.\n";
    return 1;
  }

  // parsing
  HolyLua::Parser parser(tokens, source);
  auto program = parser.parse();

  // print AST
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

  // check if compilation failed
  if (cCode.empty()) {
    std::cerr << "Compilation failed due to errors.\n";
    return 1;
  }

  // write C output
  std::string cFileName = outputName + ".c";
  std::ofstream outFile(cFileName);
  outFile << cCode;
  outFile.close();

  if (generateAsm) {
    std::string asmFileName = outputName + ".s";
    
    std::string gccCommand = "gcc -S -m64 -masm=intel "
                             "-fno-asynchronous-unwind-tables "
                             "-fno-ident "
                             "-fno-stack-protector "
                             "-O3 "
                             + cFileName + " -o " + asmFileName + 
                             " -Iinclude -L./lib -lholylua -lm 2>&1";
    
    std::cout << "Generating clean assembly file: " << asmFileName << "\n";
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
    std::string gccCommand = "gcc " + cFileName + " -o " + outputName +
                           " -Iinclude -L./lib -lholylua -lm 2>&1";

    int result = system(gccCommand.c_str());

    if (result != 0) {
      std::cerr << "Failed to compile C code with gcc.\n";
      if (!keepC) {
        std::remove(cFileName.c_str());
      }
      return 1;
    }

    // remove C file unless --keep-c flag is set
    if (!keepC) {
      std::remove(cFileName.c_str());
    }
  }

  return 0;
}