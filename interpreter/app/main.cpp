#include "chirp/frontend.h"
#include "chirp/interpreter.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifndef CHIRP_ENABLE_SOURCE_TREE_BOOT
#define CHIRP_ENABLE_SOURCE_TREE_BOOT 0
#endif

#ifndef CHIRP_SOURCE_BOOT_DIR
#define CHIRP_SOURCE_BOOT_DIR ""
#endif

#ifndef CHIRP_INSTALL_BOOT_DIR
#define CHIRP_INSTALL_BOOT_DIR ""
#endif

namespace {

namespace fs = std::filesystem;

struct Options {
    bool ast_dump = false;
    bool format = false;
    std::optional<std::string> boot_dir;
    std::string script_path;
};

void printUsage(std::ostream& out) {
    out << "Usage: chirp [--ast-dump] [--format] [--boot-dir DIR] [script]\n"
        << "\n"
        << "Options:\n"
        << "  --ast-dump      Parse the input and print its AST.\n"
        << "  --format        Rewrite ASCII operator aliases to Unicode in-place.\n"
        << "  --boot-dir DIR  Load boot .chirp files from DIR before running scripts or the REPL.\n"
        << "  --help          Show this help message.\n";
}

std::string readFile(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool formatFile(const fs::path& path) {
    try {
        std::string source = readFile(path);
        std::string formatted = chirp::frontend::format_text(source);
        if (formatted != source) {
            std::ofstream out_file(path);
            if (!out_file.is_open()) {
                throw std::runtime_error("Could not write file: " + path.string());
            }
            out_file << formatted;
            std::cout << "Formatted " << path.string() << "\n";
        } else {
            std::cout << "Already formatted " << path.string() << "\n";
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Formatting error in " << path.string() << ": " << e.what() << "\n";
        return false;
    }
}

fs::path requireBootDir(const fs::path& path, std::string_view source) {
    std::error_code ec;
    if (!fs::exists(path, ec) || ec) {
        throw std::runtime_error(std::string(source) + " boot directory not found: " + path.string());
    }
    if (!fs::is_directory(path, ec) || ec) {
        throw std::runtime_error(std::string(source) + " boot path is not a directory: " + path.string());
    }
    return path;
}

std::optional<fs::path> existingAutoBootDir(const fs::path& path) {
    if (path.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    if (fs::exists(path, ec) && !ec && fs::is_directory(path, ec) && !ec) {
        return path;
    }
    return std::nullopt;
}

std::optional<fs::path> findBootDir(const Options& options) {
    if (options.boot_dir.has_value()) {
        return requireBootDir(*options.boot_dir, "CLI");
    }

    if (const char* env = std::getenv("CHIRP_BOOT_DIR"); env != nullptr && *env != '\0') {
        return requireBootDir(env, "CHIRP_BOOT_DIR");
    }

#if CHIRP_ENABLE_SOURCE_TREE_BOOT
    if (auto boot_dir = existingAutoBootDir(CHIRP_SOURCE_BOOT_DIR)) {
        return boot_dir;
    }
#endif

    if (auto boot_dir = existingAutoBootDir(CHIRP_INSTALL_BOOT_DIR)) {
        return boot_dir;
    }

    return std::nullopt;
}

std::string bootSortKey(const fs::path& boot_dir, const fs::path& file) {
    std::error_code ec;
    fs::path relative = fs::relative(file, boot_dir, ec);
    if (!ec) {
        return relative.generic_string();
    }
    return file.generic_string();
}

std::vector<fs::path> findBootFiles(const fs::path& boot_dir) {
    std::vector<fs::path> files;
    fs::recursive_directory_iterator it(boot_dir);
    fs::recursive_directory_iterator end;
    for (; it != end; ++it) {
        if (it->is_regular_file() && it->path().extension() == ".chirp") {
            files.push_back(it->path());
        }
    }

    std::sort(files.begin(), files.end(), [&](const fs::path& left, const fs::path& right) {
        return bootSortKey(boot_dir, left) < bootSortKey(boot_dir, right);
    });
    return files;
}

void loadBoot(chirp::interpreter::Session& session, const fs::path& boot_dir) {
    for (const auto& file : findBootFiles(boot_dir)) {
        session.execute_boot_source(readFile(file), file.string());
    }
}

void loadConfiguredBoot(chirp::interpreter::Session& session, const Options& options) {
    if (auto boot_dir = findBootDir(options)) {
        loadBoot(session, *boot_dir);
    }
}

bool runAstDump(const fs::path& path) {
    try {
        std::string source = readFile(path);
        auto tokens = chirp::frontend::tokenize(source);
        auto stmts = chirp::frontend::parse(tokens);
        std::cout << chirp::frontend::print_ast(stmts);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << path.string() << ": " << e.what() << "\n";
        return false;
    }
}

bool runFile(const fs::path& path, const Options& options) {
    try {
        chirp::interpreter::Session session(std::cout);
        loadConfiguredBoot(session, options);
        session.execute_source(readFile(path), path.string());
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

bool runPrompt(const Options& options) {
    try {
        chirp::interpreter::Session session(std::cout);
        loadConfiguredBoot(session, options);

        std::string line;
        std::cout << "Chirp REPL (Experimental Syntax Stub)\n"
                  << "Type 'exit' or 'quit' to exit. Use '--ast-dump' when running a script to view ASTs.\n";
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (line == "exit" || line == "quit") {
                break;
            }
            if (line.empty()) continue;

            try {
                session.execute_source(line, "<repl>");
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

std::optional<Options> parseArgs(int argc, char* argv[]) {
    Options options;
    std::vector<std::string> args(argv + 1, argv + argc);

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(std::cout);
            return std::nullopt;
        }
        if (arg == "--ast-dump") {
            options.ast_dump = true;
            continue;
        }
        if (arg == "--format") {
            options.format = true;
            continue;
        }
        if (arg == "--boot-dir") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: --boot-dir requires a directory path.\n";
                printUsage(std::cerr);
                return std::nullopt;
            }
            options.boot_dir = args[++i];
            continue;
        }
        if (!arg.empty() && arg[0] != '-') {
            if (!options.script_path.empty()) {
                std::cerr << "Error: multiple script paths provided.\n";
                printUsage(std::cerr);
                return std::nullopt;
            }
            options.script_path = arg;
            continue;
        }

        std::cerr << "Unknown flag: " << arg << "\n";
        printUsage(std::cerr);
        return std::nullopt;
    }

    return options;
}

bool wantsHelp(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char* argv[]) {
    auto parsed = parseArgs(argc, argv);
    if (!parsed.has_value()) {
        return wantsHelp(argc, argv) ? 0 : 1;
    }

    const Options& options = *parsed;
    if (!options.script_path.empty()) {
        if (options.format) {
            return formatFile(options.script_path) ? 0 : 1;
        }
        if (options.ast_dump) {
            return runAstDump(options.script_path) ? 0 : 1;
        }
        return runFile(options.script_path, options) ? 0 : 1;
    }

    if (options.format) {
        std::cerr << "Error: --format requires a file path.\n";
        return 1;
    }
    if (options.ast_dump) {
        std::cerr << "Error: --ast-dump requires a file path.\n";
        return 1;
    }

    return runPrompt(options) ? 0 : 1;
}
