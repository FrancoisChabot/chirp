#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "chirp/parser.h"

bool ast_dump_mode = false;
bool format_mode = false;

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
        std::string formatted = chirp::parser::format_text(source);
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

void run(const std::string& source) {
    try {
        auto tokens = chirp::parser::tokenize(source);
        auto stmts = chirp::parser::parse(tokens);
        
        if (ast_dump_mode) {
            std::cout << chirp::parser::print_ast(stmts);
        } else {
            std::cout << "Parsed " << stmts.size() << " statement(s) successfully. (Use --ast-dump to view AST)\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void runPrompt() {
    std::string line;
    std::cout << "Chirp REPL (Type 'exit' to quit)\n";
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

void runFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    run(buffer.str());
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string script_path = "";

    for (const auto& arg : args) {
        if (arg == "--ast-dump") {
            ast_dump_mode = true;
        } else if (arg == "--format") {
            format_mode = true;
        } else if (arg[0] != '-') {
            script_path = arg;
        } else {
            std::cerr << "Unknown flag: " << arg << "\n";
            std::cerr << "Usage: chirp_app [--ast-dump] [--format] [script]\n";
            return 1;
        }
    }

    if (!script_path.empty()) {
        if (format_mode) {
            formatFile(script_path);
        } else {
            runFile(script_path);
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
