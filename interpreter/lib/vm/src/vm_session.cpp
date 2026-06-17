#include "chirp/vm_session.h"
#include <iostream>
#include <stdexcept>

namespace chirp {

class VMSession::Impl {
public:
    std::ostream& out;
    bool testing_enabled;
    std::string chirp_root;

    Impl(std::ostream& out, bool testing_enabled)
        : out(out), testing_enabled(testing_enabled) {}
};

VMSession::VMSession(std::ostream& out, bool testing_enabled)
    : impl_(std::make_unique<Impl>(out, testing_enabled)) {}

VMSession::~VMSession() = default;
VMSession::VMSession(VMSession&&) noexcept = default;
VMSession& VMSession::operator=(VMSession&&) noexcept = default;

void VMSession::execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
    // Stub implementation
}

void VMSession::execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) {
    // Stub implementation
}

void VMSession::execute_source(std::string source, std::string label) {
    // Stub implementation
}

void VMSession::execute_boot_source(std::string source, std::string label) {
    // Stub implementation
}

void VMSession::set_chirp_root(std::string path) {
    impl_->chirp_root = std::move(path);
}

SessionExpectations VMSession::getExpectations() const {
    return SessionExpectations{}; // Stub
}

} // namespace chirp
