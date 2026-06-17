#pragma once

#include "chirp/backend.h"
#include <iosfwd>
#include <memory>

namespace chirp {

class VMSession : public BackendSession {
public:
    explicit VMSession(std::ostream& out, bool testing_enabled = false);
    ~VMSession() override;

    VMSession(const VMSession&) = delete;
    VMSession& operator=(const VMSession&) = delete;
    VMSession(VMSession&&) noexcept;
    VMSession& operator=(VMSession&&) noexcept;

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) override;
    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) override;
    void execute_source(std::string source, std::string label) override;
    void execute_boot_source(std::string source, std::string label) override;
    void set_chirp_root(std::string path) override;

    SessionExpectations getExpectations() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace chirp
