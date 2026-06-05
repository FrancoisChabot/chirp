#pragma once

#include "value.h"
#include "type.h"
#include "binding.h"

#include <exception>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace chirp::frontend {
class Stmt;
}

namespace chirp::interpreter {

class ScriptExit : public std::exception {
public:
    explicit ScriptExit(int code) : code_(code) {}

    int code() const noexcept { return code_; }
    const char* what() const noexcept override { return "script exit"; }

private:
    int code_;
};

// Getters for the core predefined interpreter values and types

// Core Types (as std::shared_ptr<const Type>)
std::shared_ptr<const Type> getBoolType();
std::shared_ptr<const Type> getUndecidedType();
std::shared_ptr<const Type> getMetaType(); // represents the "Type" type

std::shared_ptr<const Type> getSetType();
std::shared_ptr<const Type> getVoidType();
std::shared_ptr<const Type> getEnumeratedSetType();
std::shared_ptr<const Type> getRangeType();
std::shared_ptr<const Type> getConstructedSetType();
std::shared_ptr<const Type> getCompositeSetType();
std::shared_ptr<const Type> getBindingType();
std::shared_ptr<const Type> getFunctionType();

// Convenient types for literals
std::shared_ptr<const Type> getIntType();
std::shared_ptr<const Type> getStringType();
std::shared_ptr<const Type> getSymbolType();
std::shared_ptr<const Type> getListType();


// Core Values (as Value objects)
const Value& Bool();
const Value& True();
const Value& False();
const Value& Undecided();
const Value& UndecidedVal();
const Value& TypeVal();

const Value& Set();
const Value& SetTypeVal();
const Value& Void();
const Value& VoidVal();

// Set belonging helper: typeof(S).bp(S, v)
Value belongsTo(const Value& S, const Value& v);

// Set range helper: typeof(S).br(S, lc)
Value belongsRange(const Value& S, const Value& lc);

struct SessionExpectations {
    std::optional<std::string> expected_stdout;
    std::optional<int> expected_exit;
    bool expect_test_failure = false;
    bool has_expectations = false;
};

class Session {
public:
    explicit Session(std::ostream& out);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts);
    void execute_source(std::string source, std::string label);
    void execute_boot_source(std::string source, std::string label);

    SessionExpectations getExpectations() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Execute a parsed Chirp script.
void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::ostream& out);

} // namespace chirp::interpreter
