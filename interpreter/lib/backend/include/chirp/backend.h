#pragma once

#include "chirp/frontend.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <iosfwd>

namespace chirp {

struct SessionExpectations {
    std::optional<std::string> expected_stdout;
    std::optional<std::string> expected_stderr;
    std::optional<int> expected_exit;
    bool expect_test_failure = false;
    int expectation_checks = 0;
    bool has_expectations = false;
};

class BackendSession {
public:
    virtual ~BackendSession() = default;

    virtual void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) = 0;
    virtual void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) = 0;
    virtual void execute_source(std::string source, std::string label) = 0;
    virtual void execute_boot_source(std::string source, std::string label) = 0;
    virtual void set_chirp_root(std::string path) = 0;

    virtual SessionExpectations getExpectations() const = 0;
};

} // namespace chirp
