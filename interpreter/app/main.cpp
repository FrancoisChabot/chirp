#include "chirp/frontend.h"
#include "chirp/interpreter.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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
    std::optional<std::string> run_report;
    std::string script_path;
};

void printUsage(std::ostream& out) {
    out << "Usage: chirp [--ast-dump] [--format] [--boot-dir DIR] [--run-report PATH] [script]\n"
        << "\n"
        << "Options:\n"
        << "  --ast-dump      Parse the input and print its AST.\n"
        << "  --format        Rewrite ASCII operator aliases to Unicode in-place.\n"
        << "  --boot-dir DIR  Load boot .chirp files from DIR before running scripts or the REPL.\n"
        << "  --run-report PATH\n"
        << "                  Write a structured JSON report for a script run.\n"
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

int runFile(const fs::path& path, const Options& options) {
    try {
        chirp::interpreter::Session session(std::cout);
        loadConfiguredBoot(session, options);
        session.execute_source(readFile(path), path.string());
        return 0;
    } catch (const chirp::interpreter::ScriptExit& e) {
        return e.code();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

struct Diagnostic {
    std::string phase;
    std::string message;
    std::string file;
};

struct RunReport {
    std::string outcome = "success";
    std::optional<int> script_exit;
    std::vector<Diagnostic> diagnostics;
    std::optional<std::string> expected_stdout;
    std::optional<int> expected_exit;
    bool expect_test_failure = false;
};


void writeJsonString(std::ostream& out, std::string_view text) {
    static constexpr char hex[] = "0123456789abcdef";

    out << '"';
    for (unsigned char c : text) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u00" << hex[(c >> 4) & 0x0f] << hex[c & 0x0f];
                } else {
                    out << static_cast<char>(c);
                }
                break;
        }
    }
    out << '"';
}

bool writeRunReport(std::ostream& out, const RunReport& report) {
    out << "{\"schema\": \"chirp-run-report/v2\"}\n";
    for (const auto& diagnostic : report.diagnostics) {
        out << "{\"event\": \"diagnostic\", \"phase\": ";
        writeJsonString(out, diagnostic.phase);
        out << ", \"message\": ";
        writeJsonString(out, diagnostic.message);
        out << ", \"file\": ";
        writeJsonString(out, diagnostic.file);
        out << "}\n";
    }
    out << "{\"event\": \"expectations\"";
    out << ", \"expected_stdout\": ";
    if (report.expected_stdout.has_value()) {
        writeJsonString(out, *report.expected_stdout);
    } else {
        out << "null";
    }
    out << ", \"expected_exit\": ";
    if (report.expected_exit.has_value()) {
        out << *report.expected_exit;
    } else {
        out << "null";
    }
    out << ", \"expect_test_failure\": " << (report.expect_test_failure ? "true" : "false");
    out << "}\n";

    out << "{\"event\": \"outcome\", \"outcome\": ";
    writeJsonString(out, report.outcome);
    out << ", \"script_exit\": ";
    if (report.script_exit.has_value()) {
        out << *report.script_exit;
    } else {
        out << "null";
    }
    out << "}\n";

    if (!out) {
        std::cerr << "Error: failed while writing run report\n";
        return false;
    }
    return true;
}

int runFileWithReport(const fs::path& path, const Options& options) {
    RunReport report;
    int process_exit = 0;
    std::ofstream report_file;
    std::ostream* report_out = nullptr;

    if (options.run_report.has_value()) {
        report_file.open(*options.run_report);
        if (!report_file.is_open()) {
            std::cerr << "Error: could not write run report: " << *options.run_report << "\n";
            return 1;
        }
        report_out = &report_file;
    }

    try {
        chirp::interpreter::Session session(std::cout);

        try {
            loadConfiguredBoot(session, options);
        } catch (const std::exception& e) {
            report.outcome = "boot_failure";
            report.diagnostics.push_back({"boot", e.what(), ""});
            process_exit = 1;
        }

        std::string source;
        bool loaded = false;
        if (report.diagnostics.empty()) {
            try {
                source = readFile(path);
                loaded = true;
            } catch (const std::exception& e) {
                report.outcome = "load_failure";
                report.diagnostics.push_back({"load", e.what(), path.string()});
                process_exit = 1;
            }
        }

        bool parsed = false;
        std::vector<chirp::frontend::token> tokens;
        std::vector<std::unique_ptr<chirp::frontend::Stmt>> stmts;

        if (loaded && report.diagnostics.empty()) {
            try {
                tokens = chirp::frontend::tokenize(source);
                stmts = chirp::frontend::parse(tokens);
                parsed = true;
            } catch (const std::exception& e) {
                report.outcome = "syntax_failure";
                report.diagnostics.push_back({"parse", e.what(), path.string()});
                process_exit = 1;
            }
        }

        if (parsed && report.diagnostics.empty()) {
            try {
                session.execute(stmts);
            } catch (const chirp::interpreter::ScriptExit& e) {
                report.outcome = "script_exit";
                report.script_exit = e.code();
                process_exit = e.code();
            } catch (const std::exception& e) {
                report.outcome = "evaluation_failure";
                report.diagnostics.push_back({"evaluate", e.what(), path.string()});
                process_exit = 1;
            }

            auto dynamic_expectations = session.getExpectations();
            if (dynamic_expectations.has_expectations) {
                report.expected_stdout = dynamic_expectations.expected_stdout;
                report.expected_exit = dynamic_expectations.expected_exit;
                report.expect_test_failure = dynamic_expectations.expect_test_failure;
            }
        }
    } catch (const std::exception& e) {
        report.outcome = "internal_failure";
        report.diagnostics.push_back({"internal", e.what(), ""});
        process_exit = 1;
    }

    if (report_out != nullptr) {
        if (!writeRunReport(*report_out, report)) {
            return 1;
        }
    }
    return process_exit;
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
        if (arg == "--run-report") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: --run-report requires a file path.\n";
                printUsage(std::cerr);
                return std::nullopt;
            }
            options.run_report = args[++i];
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
            if (options.run_report.has_value()) {
                std::cerr << "Error: run-report options cannot be used with --format.\n";
                return 1;
            }
            return formatFile(options.script_path) ? 0 : 1;
        }
        if (options.ast_dump) {
            if (options.run_report.has_value()) {
                std::cerr << "Error: run-report options cannot be used with --ast-dump.\n";
                return 1;
            }
            return runAstDump(options.script_path) ? 0 : 1;
        }
        if (options.run_report.has_value()) {
            return runFileWithReport(options.script_path, options);
        }
        return runFile(options.script_path, options);
    }

    if (options.run_report.has_value()) {
        std::cerr << "Error: run-report options require a script path.\n";
        return 1;
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
