#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "chirp/frontend.h"
#include "chirp/interpreter.h"

bool ast_dump_mode = false;
bool format_mode = false;

void printUsage(std::ostream& out) {
    out << "Usage: chirp [--ast-dump] [--format] [script]\n"
        << "\n"
        << "Options:\n"
        << "  --ast-dump  Parse the input and print its AST.\n"
        << "  --format    Rewrite ASCII operator aliases to Unicode in-place.\n"
        << "  --help      Show this help message.\n";
}

void formatFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string source = buffer.str();
    
    try {
        std::string formatted = chirp::frontend::format_text(source);
        if (formatted != source) {
            std::ofstream out_file(path);
            out_file << formatted;
            std::cout << "Formatted " << path << "\n";
        } else {
            std::cout << "Already formatted " << path << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Formatting error in " << path << ": " << e.what() << "\n";
    }
}

bool run(const std::string& source) {
    try {
        auto tokens = chirp::frontend::tokenize(source);
        auto stmts = chirp::frontend::parse(tokens);
        
        if (ast_dump_mode) {
            std::cout << chirp::frontend::print_ast(stmts);
        } else {
            chirp::interpreter::execute(stmts, std::cout);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

void runPrompt() {
    std::string line;
    std::cout << "Chirp REPL (Experimental Syntax Stub)\n"
              << "Type 'exit' or 'quit' to exit. Use '--ast-dump' when running a script to view ASTs.\n";
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break; // EOF
        }
        if (line == "exit" || line == "quit") {
            break;
        }
        if (line.empty()) continue;
        run(line);
    }
}

bool runFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return run(buffer.str());
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string script_path = "";

    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            printUsage(std::cout);
            return 0;
        } else if (arg == "--ast-dump") {
            ast_dump_mode = true;
        } else if (arg == "--format") {
            format_mode = true;
        } else if (!arg.empty() && arg[0] != '-') {
            script_path = arg;
        } else {
            std::cerr << "Unknown flag: " << arg << "\n";
            printUsage(std::cerr);
            return 1;
        }
    }

    if (!script_path.empty()) {
        if (format_mode) {
            formatFile(script_path);
            return 0;
        } else {
            return runFile(script_path) ? 0 : 1;
        }
    } else {
        if (format_mode) {
            std::cerr << "Error: --format requires a file path.\n";
            return 1;
        }
        runPrompt();
    }
    return 0;
}
