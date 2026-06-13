#pragma once

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace chirp::frontend {
class Stmt;
}

namespace chirp::backend {

class ScriptExit : public std::exception {
public:
    explicit ScriptExit(int code) : code_(code) {}

    int code() const noexcept { return code_; }
    const char* what() const noexcept override { return "script exit"; }

private:
    int code_;
};

struct SessionExpectations {
    std::optional<std::string> expected_stdout;
    std::optional<std::string> expected_stderr;
    std::optional<int> expected_exit;
    bool expect_test_failure = false;
    int expectation_checks = 0;
    bool has_expectations = false;
};

class Session {
public:
    virtual ~Session() = default;

    virtual void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) = 0;
    virtual void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) = 0;
    virtual void execute_source(std::string source, std::string label) = 0;
    virtual void execute_boot_source(std::string source, std::string label) = 0;
    virtual void set_chirp_root(std::string path) = 0;

    virtual SessionExpectations getExpectations() const = 0;
};

} // namespace chirp::backend
