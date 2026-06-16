#include "chirp/interpreter.h"
#include "chirp/bigint.h"
#include "chirp/frontend.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace chirp::interpreter {
namespace {

using namespace chirp::frontend;

struct BreakSignal {
    Value value;
    std::shared_ptr<Binding> handoff_binding;
};

std::string at(const token& tok) {
    return " at line " + std::to_string(tok.line) + ":" + std::to_string(tok.column);
}

[[noreturn]] void fail(const token& tok, const std::string& message) {
    throw std::runtime_error(message + at(tok));
}

std::string to_key(std::string_view text) {
    return std::string(text);
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return 10 + c - 'A';
}

void append_utf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

std::string decode_quoted_literal(std::string_view literal, const token& diag) {
    if (literal.size() < 2) {
        fail(diag, "Malformed string literal");
    }

    std::string out;
    for (size_t i = 1; i + 1 < literal.size(); ++i) {
        char c = literal[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }

        if (++i + 1 > literal.size()) {
            fail(diag, "Malformed escape sequence");
        }

        char escaped = literal[i];
        switch (escaped) {
            case '\\': out.push_back('\\'); break;
            case '\'': out.push_back('\''); break;
            case '"': out.push_back('"'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '0': out.push_back('\0'); break;
            case 'u': {
                if (i + 4 >= literal.size()) {
                    fail(diag, "Malformed unicode escape");
                }
                uint32_t codepoint = 0;
                for (int n = 0; n < 4; ++n) {
                    char h = literal[++i];
                    codepoint = (codepoint << 4) | static_cast<uint32_t>(hex_value(h));
                }
                append_utf8(out, codepoint);
                break;
            }
            default:
                fail(diag, "Unsupported escape sequence");
        }
    }
    return out;
}

std::string decode_fstring_part(std::string_view literal, frontend::token_type t, const frontend::token& diag) {
    size_t start = (t == frontend::token_type::fstring_head || t == frontend::token_type::fstring_literal) ? 2 : 1;
    size_t end = (t == frontend::token_type::fstring_tail || t == frontend::token_type::fstring_literal) ? literal.size() - 1 : literal.size() - 1;
    if (start > end) return "";

    std::string out;
    for (size_t i = start; i < end; ++i) {
        char c = literal[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }

        if (++i >= end) {
            fail(diag, "Malformed escape sequence");
        }

        char escaped = literal[i];
        switch (escaped) {
            case '\\': out.push_back('\\'); break;
            case '\'': out.push_back('\''); break;
            case '"': out.push_back('"'); break;
            case '{': out.push_back('{'); break;
            case '}': out.push_back('}'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '0': out.push_back('\0'); break;
            case 'u': {
                if (i + 4 >= end) {
                    fail(diag, "Malformed unicode escape");
                }
                uint32_t codepoint = 0;
                for (int n = 0; n < 4; ++n) {
                    char h = literal[++i];
                    codepoint = (codepoint << 4) | static_cast<uint32_t>(hex_value(h));
                }
                append_utf8(out, codepoint);
                break;
            }
            default:
                fail(diag, "Unsupported escape sequence");
        }
    }
    return out;
}

uint32_t decode_utf8_char(std::string_view str, const frontend::token& diag) {
    if (str.empty()) {
        fail(diag, "Empty character literal");
    }
    unsigned char c1 = str[0];
    if (c1 < 0x80) {
        return c1;
    } else if ((c1 & 0xE0) == 0xC0) {
        if (str.size() < 2) fail(diag, "Malformed UTF-8 in character literal");
        unsigned char c2 = str[1];
        return ((c1 & 0x1F) << 6) | (c2 & 0x3F);
    } else if ((c1 & 0xF0) == 0xE0) {
        if (str.size() < 3) fail(diag, "Malformed UTF-8 in character literal");
        unsigned char c2 = str[1];
        unsigned char c3 = str[2];
        return ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    } else if ((c1 & 0xF8) == 0xF0) {
        if (str.size() < 4) fail(diag, "Malformed UTF-8 in character literal");
        unsigned char c2 = str[1];
        unsigned char c3 = str[2];
        unsigned char c4 = str[3];
        return ((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
    }
    fail(diag, "Malformed UTF-8 in character literal");
    return 0;
}

std::string display_string(const Value& value) {
    if (value.isString()) {
        return value.asString();
    }
    return value.toString();
}

bool is_name(std::string_view actual, std::string_view expected) {
    return actual == expected;
}

constexpr int64_t MAX_LOOP_ITERATIONS = 1'000'000;

BigInt parse_positive_integer_literal(std::string_view text, const token& diag) {
    if (text.find('.') != std::string_view::npos) {
        fail(diag, "Floating point literals are not supported yet");
    }
    try {
        return BigInt(text);
    } catch (const std::exception& e) {
        fail(diag, "Invalid integer literal '" + std::string(text) + "'");
    }
}

BigInt parse_negated_integer_literal(std::string_view text, const token& diag) {
    if (text.find('.') != std::string_view::npos) {
        fail(diag, "Floating point literals are not supported yet");
    }
    try {
        std::string neg_text = "-" + std::string(text);
        return BigInt(neg_text);
    } catch (const std::exception& e) {
        fail(diag, "Invalid integer literal '-" + std::string(text) + "'");
    }
}

class Evaluator : public ASTVisitor, public StmtVisitor {
public:
    SessionExpectations expectations;

    explicit Evaluator(std::ostream& out, bool testing_enabled = false)
        : out_(out), testing_enabled_(testing_enabled) {
        scopes_.push_back(std::make_shared<RuntimeScope>());
    }

    void execute(const std::vector<std::unique_ptr<Stmt>>& stmts) {
        close_boot_private_scope();
        execute_statements(stmts);
    }

    void execute(const std::vector<std::unique_ptr<Stmt>>& stmts, std::string label) {
        close_boot_private_scope();
        source_stack_.push_back(std::move(label));
        try {
            execute_statements(stmts);
            source_stack_.pop_back();
        } catch (...) {
            source_stack_.pop_back();
            throw;
        }
    }

    void execute_boot(const std::vector<std::unique_ptr<Stmt>>& stmts, std::string label) {
        ensure_boot_private_scope();
        bool was_boot_mode = boot_mode_;
        boot_mode_ = true;
        source_stack_.push_back(std::move(label));
        try {
            execute_statements(stmts);
            source_stack_.pop_back();
            boot_mode_ = was_boot_mode;
        } catch (...) {
            source_stack_.pop_back();
            boot_mode_ = was_boot_mode;
            throw;
        }
    }

    void set_chirp_root(std::string path) {
        chirp_root_ = std::filesystem::absolute(std::filesystem::path(std::move(path))).lexically_normal();
    }

    void shutdown() {
        std::vector<ScopePtr> reachable_scopes;
        std::unordered_set<const RuntimeScope*> seen_scopes;
        std::unordered_set<const Binding*> seen_bindings;
        std::unordered_set<const Value::HeapAllocationState*> seen_heap_allocations;

        for (const auto& scope : scopes_) {
            collect_scope_references(scope, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        }
        for (auto& [_, value] : registered_items_) {
            collect_value_references(value, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        }
        for (auto& implementation : implementations_) {
            collect_value_references(implementation.trait, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            collect_value_references(implementation.impl, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        }
        for (auto& [_, entry] : module_cache_) {
            collect_value_references(entry.value, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        }
        collect_value_references(result_, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        if (terminal_handoff_.has_value()) {
            collect_binding_references(terminal_handoff_->binding, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        }

        for (const auto& scope : reachable_scopes) {
            if (!scope) {
                continue;
            }
            scope->bindings.clear();
            scope->declaration_order.clear();
        }

        terminal_handoff_.reset();
        result_ = VoidVal();

        for (const auto& scope : scopes_) {
            if (!scope) {
                continue;
            }
            scope->bindings.clear();
            scope->declaration_order.clear();
        }
        scopes_.clear();

        module_export_stack_.clear();
        module_cache_.clear();
        registered_items_.clear();
        implementations_.clear();
        lambda_param_struct_types_.clear();
        construction_arg_struct_types_.clear();

        source_stack_.clear();
        module_sources_.clear();
        chirp_root_.reset();
        injected_stdin_.clear();
        stdin_cursor_ = 0;
    }

private:
    using Scope = RuntimeScope;
    using ScopePtr = std::shared_ptr<Scope>;

    struct SourceUnit {
        std::string label;
        std::string source;
        std::vector<std::unique_ptr<frontend::Stmt>> stmts;
    };

    struct ModuleCacheEntry {
        enum class State { Loading, Loaded, Failed };

        State state = State::Loading;
        Value value;
        std::string error;
    };

    std::ostream& out_;
    RuntimeScopeChain scopes_;
    std::unordered_map<std::string, Value> registered_items_;
    Value result_;
    bool boot_mode_ = false;
    bool boot_private_scope_active_ = false;
    uint64_t next_mint_id_ = 1;
    uint64_t next_trait_id_ = 1;
    uint64_t next_heap_allocation_id_ = 1;
    bool testing_enabled_ = false;
    bool stdin_injected_ = false;
    std::string injected_stdin_;
    size_t stdin_cursor_ = 0;
    struct TerminalHandoff {
        size_t depth;
        std::shared_ptr<Binding> binding;
    };

    struct TerminalValue {
        Value value;
        std::shared_ptr<Binding> handoff_binding;
    };

    std::optional<TerminalHandoff> terminal_handoff_;
    std::optional<std::filesystem::path> chirp_root_;
    std::unordered_map<std::string, ModuleCacheEntry> module_cache_;
    std::vector<std::unique_ptr<SourceUnit>> module_sources_;
    std::vector<std::string> source_stack_;
    std::vector<std::map<std::string, std::shared_ptr<Binding>>*> module_export_stack_;
    std::unordered_map<const frontend::LambdaExpr*, std::shared_ptr<const RuntimeStructType>> lambda_param_struct_types_;
    std::unordered_map<const Type*, std::shared_ptr<const RuntimeStructType>> construction_arg_struct_types_;

    struct Implementation {
        Value trait;
        std::shared_ptr<const Type> on;
        Value impl;
    };
    std::vector<Implementation> implementations_;

    void collect_scope_references(
        const ScopePtr& scope,
        std::vector<ScopePtr>& reachable_scopes,
        std::unordered_set<const RuntimeScope*>& seen_scopes,
        std::unordered_set<const Binding*>& seen_bindings,
        std::unordered_set<const Value::HeapAllocationState*>& seen_heap_allocations) {
        if (!scope) {
            return;
        }

        const RuntimeScope* scope_ptr = scope.get();
        if (!seen_scopes.insert(scope_ptr).second) {
            return;
        }

        reachable_scopes.push_back(scope);
        for (const auto& [_, binding] : scope->bindings) {
            collect_binding_references(binding, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        }
        for (const auto& binding : scope->declaration_order) {
            collect_binding_references(binding, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        }
    }

    void collect_binding_references(
        const std::shared_ptr<Binding>& binding,
        std::vector<ScopePtr>& reachable_scopes,
        std::unordered_set<const RuntimeScope*>& seen_scopes,
        std::unordered_set<const Binding*>& seen_bindings,
        std::unordered_set<const Value::HeapAllocationState*>& seen_heap_allocations) {
        if (!binding) {
            return;
        }

        const Binding* binding_ptr = binding.get();
        if (!seen_bindings.insert(binding_ptr).second) {
            return;
        }

        collect_value_references(const_cast<Value&>(binding->getFC()), reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        collect_value_references(const_cast<Value&>(binding->getLC()), reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
        collect_value_references(const_cast<Value&>(binding->getCV()), reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
    }

    void collect_value_references(
        Value& value,
        std::vector<ScopePtr>& reachable_scopes,
        std::unordered_set<const RuntimeScope*>& seen_scopes,
        std::unordered_set<const Binding*>& seen_bindings,
        std::unordered_set<const Value::HeapAllocationState*>& seen_heap_allocations) {
        if (value.isBinding()) {
            collect_binding_references(value.asBinding(), reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            return;
        }
        if (value.isEnumeratedSet()) {
            for (auto& element : const_cast<std::vector<Value>&>(value.asEnumeratedSet())) {
                collect_value_references(element, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            return;
        }
        if (value.isRange()) {
            auto range = value.asRange();
            if (range.start) {
                collect_value_references(*range.start, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            if (range.end) {
                collect_value_references(*range.end, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            return;
        }
        if (value.isConstructedSet()) {
            const auto& tag = value.asConstructedSetTag();
            if (tag.captured_scopes) {
                for (const auto& scope : *tag.captured_scopes) {
                    collect_scope_references(scope, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
                }
            }
            return;
        }
        if (value.isList()) {
            for (auto& element : const_cast<std::vector<Value>&>(value.asList())) {
                collect_value_references(element, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            return;
        }
        if (value.isLambda()) {
            const auto& tag = value.asLambdaTag();
            if (tag.captured_scopes) {
                for (const auto& scope : *tag.captured_scopes) {
                    collect_scope_references(scope, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
                }
            }
            return;
        }
        if (value.isSignature()) {
            const auto& tag = value.asSignatureTag();
            if (tag.captured_scopes) {
                for (const auto& scope : *tag.captured_scopes) {
                    collect_scope_references(scope, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
                }
            }
            return;
        }
        if (value.isCompositeSet()) {
            const auto& tag = value.asCompositeSet();
            if (tag.left) {
                collect_value_references(*tag.left, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            if (tag.right) {
                collect_value_references(*tag.right, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            return;
        }
        if (value.isComplementSet()) {
            const auto& tag = value.asComplementSet();
            if (tag.operand) {
                collect_value_references(*tag.operand, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            return;
        }
        if (value.isTrait()) {
            collect_value_references(const_cast<Value&>(value.asTraitInterface()), reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            return;
        }
        if (value.isStructInstance()) {
            for (auto& [_, field_value] : *value.asStructInstance().fields) {
                collect_value_references(field_value, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            return;
        }
        if (value.isModule()) {
            const auto& exports = value.asModule().exports;
            if (!exports) {
                return;
            }
            for (const auto& [_, binding] : *exports) {
                collect_binding_references(binding, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
            return;
        }
        if (value.isHeapAllocation()) {
            const auto& state = value.asHeapAllocation().state;
            if (!state) {
                return;
            }
            const Value::HeapAllocationState* state_ptr = state.get();
            if (!seen_heap_allocations.insert(state_ptr).second) {
                return;
            }
            if (state->stored) {
                collect_value_references(*state->stored, reachable_scopes, seen_scopes, seen_bindings, seen_heap_allocations);
            }
        }
    }

    void execute_statements(const std::vector<std::unique_ptr<Stmt>>& stmts) {
        for (const auto& stmt : stmts) {
            execute_stmt(*stmt);
        }
    }

    void ensure_boot_private_scope() {
        if (boot_private_scope_active_) {
            return;
        }
        scopes_.push_back(std::make_shared<Scope>());
        boot_private_scope_active_ = true;
    }

    void close_boot_private_scope() {
        if (!boot_private_scope_active_) {
            return;
        }
        if (scopes_.size() != 2) {
            throw std::runtime_error("Cannot close boot private scope while nested scopes are active");
        }
        scopes_.pop_back();
        boot_private_scope_active_ = false;
    }

    bool is_boot_top_level() const {
        return boot_mode_ && boot_private_scope_active_ && scopes_.size() == 2;
    }

    bool is_module_top_level() const {
        return !module_export_stack_.empty() && scopes_.size() == 2;
    }

    std::shared_ptr<const RuntimeScopeChain> capture_scopes() const {
        return std::make_shared<RuntimeScopeChain>(scopes_);
    }

    class PurityVisitor : public frontend::ASTVisitor, public frontend::StmtVisitor {
    public:
        Evaluator& evaluator;
        bool is_pure = true;
        const std::vector<frontend::NamedBinding>& params;
        std::set<std::string> local_vars;

        PurityVisitor(Evaluator& eval, const std::vector<frontend::NamedBinding>& p)
            : evaluator(eval), params(p) {}

        void visit(const frontend::CallExpr& expr) override {
            if (!is_pure) return;
            std::string name;
            if (auto* ident = dynamic_cast<const frontend::IdentifierExpr*>(expr.callee.get())) {
                name = std::string(ident->name);
            } else if (auto* intrinsic = dynamic_cast<const frontend::IntrinsicExpr*>(expr.callee.get())) {
                name = std::string(intrinsic->name);
            } else {
                is_pure = false; return;
            }

            bool is_param = false;
            for (const auto& p : params) {
                if (std::string(p.name.lexeme) == name) {
                    is_param = true; break;
                }
            }
            if (is_param) {
                is_pure = false; return;
            }
            if (local_vars.count(name)) {
                is_pure = false; return;
            }
            auto binding = evaluator.lookup_binding_optional(name);
            if (binding) {
                Value val = binding->getCV();
                if (val.isLambda()) {
                        if (!evaluator.check_purity(val.asLambdaTag())) {
                            is_pure = false; return;
                        }
                } else if (val.isHostFunction()) {
                    if (val.asHostFunction() == Value::HostFunction::Print) {
                        is_pure = false; return;
                    }
                } else if (val.isEnumeratedSet() && binding->isFinal()) {
                    // Capturing a final boolean/enumerated set is pure
                } else if (!val.isType()) {
                    is_pure = false; return;
                }
            } else {
                is_pure = false; return;
            }

            for (const auto& arg : expr.args) {
                if (arg.value) arg.value->accept(*this);
            }
        }

        void visit(const frontend::AssignStmt& stmt) override {
            if (!is_pure) return;
            auto* ident = dynamic_cast<const frontend::IdentifierExpr*>(stmt.target.get());
            if (!ident) {
                is_pure = false; return;
            }
            std::string name = std::string(ident->name);
            bool is_local = local_vars.count(name) > 0;
            if (!is_local) {
                for (const auto& p : params) {
                    if (std::string(p.name.lexeme) == name) {
                        is_local = true; break;
                    }
                }
            }
            if (!is_local) {
                is_pure = false; return;
            }
            if (stmt.value) stmt.value->accept(*this);
        }

        void visit(const frontend::LetStmt& stmt) override {
            if (!is_pure) return;
            local_vars.insert(std::string(stmt.binding.name.lexeme));
            if (stmt.binding.initializer) stmt.binding.initializer->accept(*this);
        }

        void visit(const frontend::ForExpr& expr) override {
            if (!is_pure) return;
            local_vars.insert(std::string(expr.iterator_binding.name.lexeme));
            if (expr.iterable) expr.iterable->accept(*this);
            if (expr.body) expr.body->accept(*this);
        }


        void visit(const frontend::IdentifierExpr&) override {}
        void visit(const frontend::BinaryExpr& expr) override {
            expr.left->accept(*this);
            expr.right->accept(*this);
        }
        void visit(const frontend::ListExpr& expr) override {
            for (const auto& e : expr.elements) e->accept(*this);
        }
        void visit(const frontend::LambdaExpr& expr) override {
            if (!is_pure) return;
            for (const auto& param : expr.parameters) {
                if (param.type_bound) param.type_bound->accept(*this);
            }
            if (expr.return_bound) expr.return_bound->accept(*this);
            // body purity is evaluated lazily, so we don't traverse it here
        }

        void visit(const frontend::SignatureExpr& expr) override {
            if (!is_pure) return;
            for (const auto& param : expr.parameters) {
                if (param.type_bound) param.type_bound->accept(*this);
            }
            if (expr.return_bound) expr.return_bound->accept(*this);
        }

        void visit(const frontend::StructExpr& expr) override {
            for (const auto& f : expr.fields) {
                if (f.initializer) f.initializer->accept(*this);
            }
        }
        void visit(const frontend::ConstructedSetExpr& expr) override {
            local_vars.insert(std::string(expr.binding.name.lexeme));
            if (expr.binding.initializer) expr.binding.initializer->accept(*this);
            if (expr.binding.type_bound) expr.binding.type_bound->accept(*this);
            if (expr.condition) expr.condition->accept(*this);
        }
        void visit(const frontend::AnonymousStructLiteralExpr& expr) override {
            for (const auto& field : expr.fields) {
                field.value->accept(*this);
            }
        }
        void visit(const frontend::BlockExpr& expr) override {
            for (const auto& s : expr.statements) s->accept(*this);
        }
        void visit(const frontend::IfStmt& stmt) override {
            stmt.condition->accept(*this);
            stmt.then_branch->accept(*this);
            if (stmt.else_branch) stmt.else_branch->accept(*this);
        }
        void visit(const frontend::ExprStmt& stmt) override {
            stmt.expression->accept(*this);
        }
        void visit(const frontend::BreakStmt&) override {}
        void visit(const frontend::DebugStmt&) override {}
        void visit(const frontend::IntrinsicExpr&) override {}

        void visit(const frontend::UnaryExpr& expr) override {
            expr.right->accept(*this);
        }
        void visit(const frontend::GroupingExpr& expr) override {
            expr.expression->accept(*this);
        }
        void visit(const frontend::NumberExpr&) override {}
        void visit(const frontend::StringExpr&) override {}
        void visit(const frontend::FStringExpr& expr) override {
            for (const auto& part : expr.parts) part->accept(*this);
        }
        void visit(const frontend::CharExpr&) override {}


        void visit(const frontend::SymbolicConstantExpr&) override {}
        void visit(const frontend::EnumExpr&) override {}
        void visit(const frontend::EnumeratedSetExpr& expr) override {
            for (const auto& e : expr.elements) e->accept(*this);
        }
        void visit(const frontend::IfExpr& expr) override {
            expr.condition->accept(*this);
            expr.then_branch->accept(*this);
            if (expr.else_branch) expr.else_branch->accept(*this);
        }
        void visit(const frontend::WhileExpr& expr) override {
            expr.condition->accept(*this);
            expr.body->accept(*this);
        }
        void visit(const frontend::IndexExpr& expr) override {
            expr.target->accept(*this);
            for (const auto& a : expr.args) {
                if (a.value) a.value->accept(*this);
            }
        }
        void visit(const frontend::MatchExpr& expr) override {
            expr.subject->accept(*this);
            for (const auto& arm : expr.arms) {
                arm.body->accept(*this);
            }
        }
    };

    bool check_purity(const frontend::LambdaExpr* lambda) {
        if (lambda->purity_state == frontend::PurityState::Pure) return true;
        if (lambda->purity_state == frontend::PurityState::Unpure) return false;
        if (lambda->purity_state == frontend::PurityState::Checking) return true;

        lambda->purity_state = frontend::PurityState::Checking;
        PurityVisitor visitor(*this, lambda->parameters);
        if (lambda->body) lambda->body->accept(visitor);
        
        lambda->purity_state = visitor.is_pure ? frontend::PurityState::Pure : frontend::PurityState::Unpure;
        return visitor.is_pure;
    }

    bool check_purity(const Value::LambdaTag& lambda_tag) {
        RuntimeScopeChain saved_scopes = std::move(scopes_);
        if (lambda_tag.captured_scopes) {
            scopes_ = *lambda_tag.captured_scopes;
        } else {
            scopes_ = saved_scopes;
        }

        try {
            bool result = check_purity(lambda_tag.lambda);
            scopes_ = std::move(saved_scopes);
            return result;
        } catch (...) {
            scopes_ = std::move(saved_scopes);
            throw;
        }
    }

    Value evaluate(const Expr& expr) {
        expr.accept(*this);
        return result_;
    }

    Value evaluate_terminal_expression_impl(const Expr& expr) {
        if (const auto* identifier = dynamic_cast<const IdentifierExpr*>(&expr)) {
            auto [binding, depth] = lookup_binding_with_depth(identifier->name, identifier->diagnostic_token);
            if (depth == scopes_.size() - 1 &&
                binding->ownsCV() &&
                (is_uncopyable(binding->getCV().getType()) ||
                    is_shared_heap_allocation_type(binding->getCV().getType()))) {
                terminal_handoff_ = TerminalHandoff{depth, binding};
            }
            return builtin_identifier(identifier->name, identifier->diagnostic_token);
        }

        if (const auto* grouping = dynamic_cast<const GroupingExpr*>(&expr)) {
            return evaluate_terminal_expression_impl(*grouping->expression);
        }

        if (const auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
            if (as_bool(evaluate(*if_expr->condition), if_expr->diagnostic_token)) {
                return evaluate_terminal_expression_impl(*if_expr->then_branch);
            }
            return evaluate_terminal_expression_impl(*if_expr->else_branch);
        }

        if (const auto* match_expr = dynamic_cast<const MatchExpr*>(&expr)) {
            Value subject = evaluate(*match_expr->subject);

            for (const auto& arm : match_expr->arms) {
                Value pattern = evaluate(*arm.pattern);

                bool matched = false;
                if (has_setness(pattern)) {
                    Value result = belongs_to(pattern, subject, match_expr->diagnostic_token);
                    matched = as_bool(result, match_expr->diagnostic_token);
                } else {
                    matched = (subject == pattern);
                }

                if (matched) {
                    return evaluate_terminal_expression_impl(*arm.body);
                }
            }

            fail(match_expr->diagnostic_token, "Non-exhaustive match: no arm matched value " + display_string(subject));
        }

        return evaluate(expr);
    }

    TerminalValue evaluate_terminal_expression(const Expr& expr) {
        auto previous_handoff = terminal_handoff_;
        terminal_handoff_.reset();

        try {
            Value value = evaluate_terminal_expression_impl(expr);
            std::shared_ptr<Binding> handoff_binding;
            if (terminal_handoff_.has_value()) {
                handoff_binding = terminal_handoff_->binding;
            }
            terminal_handoff_ = previous_handoff;
            return TerminalValue{std::move(value), std::move(handoff_binding)};
        } catch (...) {
            terminal_handoff_ = previous_handoff;
            throw;
        }
    }

    void execute_stmt(const Stmt& stmt) {
        stmt.accept(*this);
    }

    std::pair<std::shared_ptr<Binding>, size_t> lookup_binding_with_depth(std::string_view name, const token& diag) const {
        std::string key = to_key(name);
        size_t depth = scopes_.size() - 1;
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope, --depth) {
            auto found = (*scope)->bindings.find(key);
            if (found != (*scope)->bindings.end()) {
                return {found->second, depth};
            }
        }
        fail(diag, "Undefined identifier '" + key + "'");
    }

    std::shared_ptr<Binding> lookup_binding(std::string_view name, const token& diag) const {
        std::string key = to_key(name);
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            auto found = (*scope)->bindings.find(key);
            if (found != (*scope)->bindings.end()) {
                return found->second;
            }
        }
        fail(diag, "Undefined identifier '" + key + "'");
    }

    std::shared_ptr<Binding> lookup_binding_optional(std::string_view name) const {
        std::string key = to_key(name);
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            auto found = (*scope)->bindings.find(key);
            if (found != (*scope)->bindings.end()) {
                return found->second;
            }
        }
        return nullptr;
    }

    void define_binding(std::string_view name, std::shared_ptr<Binding> binding, const token& diag) {
        if (diag.type == token_type::intrinsic && !is_boot_top_level()) {
            fail(diag, "Backtick-prefixed bindings may only be defined by top-level boot files");
        }

        std::string key = to_key(name);
        if (!scopes_.empty()) {
            for (auto scope = scopes_.begin(); scope != std::prev(scopes_.end()); ++scope) {
                auto found = (*scope)->bindings.find(key);
                if (found != (*scope)->bindings.end() && found->second->isFinal()) {
                    fail(diag, "Identifier '" + key + "' cannot shadow final binding");
                }
            }
        }

        auto [it, inserted] = scopes_.back()->bindings.emplace(key, std::move(binding));
        if (!inserted) {
            fail(diag, "Identifier '" + key + "' is already defined in this scope");
        }
        scopes_.back()->declaration_order.push_back(it->second);
    }

    void publish_global_binding(std::string_view name, std::shared_ptr<Binding> binding, const token& diag) {
        std::string key = to_key(name);
        auto [it, inserted] = scopes_.front()->bindings.emplace(key, std::move(binding));
        if (!inserted) {
            fail(diag, "Public boot identifier '" + key + "' is already defined globally");
        }
        scopes_.front()->declaration_order.push_back(it->second);
    }

    static bool as_bool(const Value& value, const token& diag) {
        if (!value.isBool()) {
            fail(diag, "Expected Bool, got " + value.toString());
        }
        return value.asBool();
    }

    static BigInt as_int(const Value& value, const token& diag) {
        if (!value.isInt()) {
            fail(diag, "Expected int, got " + value.toString());
        }
        return value.asInt();
    }

    static const ReferenceType* as_reference_type(const std::shared_ptr<const Type>& type) {
        return dynamic_cast<const ReferenceType*>(type.get());
    }

    bool is_reference_type(const std::shared_ptr<const Type>& type) const {
        return as_reference_type(type) != nullptr;
    }

    bool is_mutable_reference_type(const std::shared_ptr<const Type>& type) const {
        if (const auto* reference_type = as_reference_type(type)) {
            return reference_type->is_mut();
        }
        return false;
    }

    const Value* builtin_reference_impl_for(const Value& trait, const std::shared_ptr<const Type>& dispatch_target) const {
        if (!is_reference_type(dispatch_target)) {
            return nullptr;
        }

        if (const Value* dereferenceable_trait = get_registered_item("traits.dereferenceable")) {
            if (trait == *dereferenceable_trait) {
                return get_registered_item("traits.dereferenceable.reference_impl");
            }
        }

        if (const Value* dereferenceable_mut_trait = get_registered_item("traits.dereferenceable_mut")) {
            if (trait == *dereferenceable_mut_trait && is_mutable_reference_type(dispatch_target)) {
                return get_registered_item("traits.dereferenceable_mut.reference_impl");
            }
        }

        return nullptr;
    }

    const Value* registered_impl_for(const Value& trait, const std::shared_ptr<const Type>& dispatch_target) const {
        if (const Value* builtin_impl = builtin_reference_impl_for(trait, dispatch_target)) {
            return builtin_impl;
        }
        for (const auto& implementation : implementations_) {
            if (implementation.trait == trait && implementation.on == dispatch_target) {
                return &implementation.impl;
            }
        }
        return nullptr;
    }

    const Value* registered_setness_impl_for(const std::shared_ptr<const Type>& dispatch_target) const {
        if (const Value* st = get_registered_item("traits.set")) {
            return registered_impl_for(*st, dispatch_target);
        }
        return nullptr;
    }

    const Value* get_registered_item(const std::string& name) const {
        auto it = registered_items_.find(name);
        if (it != registered_items_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const Value* callable_trait_value() const {
        return get_registered_item("traits.callable");
    }

    const Value* unique_trait_value() const {
        return get_registered_item("traits.unique");
    }

    const Value* registered_unique_impl_for(const std::shared_ptr<const Type>& dispatch_target) const {
        const Value* trait = unique_trait_value();
        if (trait == nullptr) {
            return nullptr;
        }
        return registered_impl_for(*trait, dispatch_target);
    }

    const Value* drop_trait_value() const {
        return get_registered_item("traits.drop");
    }

    const Value* registered_drop_impl_for(const std::shared_ptr<const Type>& dispatch_target) const {
        const Value* trait = drop_trait_value();
        if (trait == nullptr) {
            return nullptr;
        }
        return registered_impl_for(*trait, dispatch_target);
    }

    bool has_drop(const std::shared_ptr<const Type>& type) const {
        return registered_drop_impl_for(type) != nullptr;
    }
    
    const Value* comparable_trait_value() const { return get_registered_item("operators.comparable"); }
    const Value* additive_trait_value() const { return get_registered_item("operators.additive"); }
    const Value* subtractive_trait_value() const { return get_registered_item("operators.subtractive"); }
    const Value* negatable_trait_value() const { return get_registered_item("operators.negatable"); }
    const Value* multiplicative_trait_value() const { return get_registered_item("operators.multiplicative"); }
    const Value* divisible_trait_value() const { return get_registered_item("operators.divisible"); }
    const Value* modulable_trait_value() const { return get_registered_item("operators.modulable"); }
    const Value* indexable_trait_value() const { return get_registered_item("operators.indexable"); }
    const Value* index_assignable_trait_value() const { return get_registered_item("operators.index_assignable"); }

    const Value* comparison_result_value(std::string_view key) const {
        return get_registered_item(std::string(key));
    }

    std::optional<Value> call_comparable(const Value& left, const Value& right, const token& diag) {
        if (left.getType() != right.getType()) {
            return std::nullopt;
        }
        const Value* trait = comparable_trait_value();
        if (trait == nullptr) {
            return std::nullopt;
        }
        const Value* impl = registered_impl_for(*trait, left.getType());
        if (impl == nullptr) {
            return std::nullopt;
        }
        if (!impl->isStructInstance()) {
            fail(diag, "`Comparable implementation must be a struct instance");
        }
        const auto& fields = *impl->asStructInstance().fields;
        auto found = fields.find("compare");
        if (found == fields.end()) {
            fail(diag, "`Comparable implementation is missing method 'compare'");
        }
                    return call_callable_with_values(found->second, {left, right}, diag, {}, {false, true});
    }

    bool comparison_result_is(const Value& value, std::string_view key, const token& diag) const {
        const Value* registered = comparison_result_value(key);
        if (registered == nullptr) {
            fail(diag, "Comparison result value '" + std::string(key) + "' is not registered");
        }
        return value == *registered;
    }

    const Value* arithmetic_trait_for(BinaryOp op) const {
        switch (op) {
            case BinaryOp::Add: return additive_trait_value();
            case BinaryOp::Sub: return subtractive_trait_value();
            case BinaryOp::Mul: return multiplicative_trait_value();
            case BinaryOp::Div: return divisible_trait_value();
            case BinaryOp::Mod: return modulable_trait_value();
            default: return nullptr;
        }
    }

    const char* arithmetic_method_for(BinaryOp op) const {
        switch (op) {
            case BinaryOp::Add: return "add";
            case BinaryOp::Sub: return "sub";
            case BinaryOp::Mul: return "mul";
            case BinaryOp::Div: return "div";
            case BinaryOp::Mod: return "mod";
            default: return nullptr;
        }
    }

    Value value_arithmetic(const Value& left, const Value& right, BinaryOp op, const token& diag) {
        if (left.isInt() && right.isInt()) {
            return Value::make_int(binary_int(left, right, op, diag));
        }
        if (left.getType() == right.getType()) {
            const Value* trait = arithmetic_trait_for(op);
            const char* method_name = arithmetic_method_for(op);
            if (trait != nullptr && method_name != nullptr) {
                const Value* impl = registered_impl_for(*trait, left.getType());
                if (impl != nullptr) {
                    if (!impl->isStructInstance()) {
                        fail(diag, "Arithmetic implementation must be a struct instance");
                    }
                    const auto& fields = *impl->asStructInstance().fields;
                    auto it = fields.find(method_name);
                    if (it == fields.end()) {
                        fail(diag, std::string("Arithmetic implementation is missing method '") + method_name + "'");
                    }
                    return call_callable_with_values(it->second, {left, right}, diag, {}, {false, true});
                }
            }
        }
        std::string op_str;
        switch (op) {
            case BinaryOp::Add: op_str = "+"; break;
            case BinaryOp::Sub: op_str = "-"; break;
            case BinaryOp::Mul: op_str = "*"; break;
            case BinaryOp::Div: op_str = "/"; break;
            case BinaryOp::Mod: op_str = "%"; break;
            default: op_str = "?"; break;
        }
        fail(diag, "Cannot perform arithmetic operation '" + op_str + "' on types " + std::string(left.getType()->name()) + " and " + std::string(right.getType()->name()));
        return Value();
    }

    Value value_negate(const Value& value, const token& diag) {
        if (value.isInt()) {
            try {
                return Value::make_int(-value.asInt());
            } catch (const std::out_of_range&) {
                fail(diag, "Integer negation overflow");
            }
        }
        const Value* trait = negatable_trait_value();
        if (trait != nullptr) {
            const Value* impl = registered_impl_for(*trait, value.getType());
            if (impl != nullptr) {
                if (!impl->isStructInstance()) {
                    fail(diag, "`Negatable implementation must be a struct instance");
                }
                const auto& fields = *impl->asStructInstance().fields;
                auto found = fields.find("neg");
                if (found == fields.end()) {
                    fail(diag, "`Negatable implementation is missing method 'neg'");
                }
                return call_callable_with_values(found->second, {value}, diag, {}, {false});
            }
        }
        fail(diag, "Cannot negate value of type " + std::string(value.getType()->name()));
        return Value();
    }

    bool value_equality(const Value& left, const Value& right, const token& diag) {
        if (left.getType() == right.getType()) {
            if (auto compare_result = call_comparable(left, right, diag)) {
                return comparison_result_is(*compare_result, "operators.comparable.equal", diag);
            }
            return left == right;
        }
        return false;
    }

    bool value_compare_less(const Value& left, const Value& right, const token& diag) {
        if (left.isEnumVariant() && right.isEnumVariant()) {
            if (left.asEnumVariant().enum_node_id != right.asEnumVariant().enum_node_id) {
                fail(diag, "Cannot compare enum variants of different enum families");
            }
            return left.asEnumVariant().index < right.asEnumVariant().index;
        }
        if (left.isInt() && right.isInt()) {
            return left.asInt() < right.asInt();
        }
        if (left.isChar() && right.isChar()) {
            return left.asChar() < right.asChar();
        }
        if (left.isString() && right.isString()) {
            return left.asString() < right.asString();
        }
        if (left.getType() == right.getType()) {
            if (auto compare_result = call_comparable(left, right, diag)) {
                return comparison_result_is(*compare_result, "operators.comparable.less", diag);
            }
        }
        fail(diag, "Cannot compare values of types " + std::string(left.getType()->name()) + " and " + std::string(right.getType()->name()));
        return false;
    }

    bool value_compare_less_equal(const Value& left, const Value& right, const token& diag) {
        if (left.isEnumVariant() && right.isEnumVariant()) {
            if (left.asEnumVariant().enum_node_id != right.asEnumVariant().enum_node_id) {
                fail(diag, "Cannot compare enum variants of different enum families");
            }
            return left.asEnumVariant().index <= right.asEnumVariant().index;
        }
        if (left.isInt() && right.isInt()) {
            return left.asInt() <= right.asInt();
        }
        if (left.isChar() && right.isChar()) {
            return left.asChar() <= right.asChar();
        }
        if (left.isString() && right.isString()) {
            return left.asString() <= right.asString();
        }
        if (left.getType() == right.getType()) {
            if (auto compare_result = call_comparable(left, right, diag)) {
                return comparison_result_is(*compare_result, "operators.comparable.less", diag) ||
                    comparison_result_is(*compare_result, "operators.comparable.equal", diag);
            }
            if (left == right) {
                return true;
            }
            return value_compare_less(left, right, diag);
        }
        fail(diag, "Cannot compare values of types " + std::string(left.getType()->name()) + " and " + std::string(right.getType()->name()));
        return false;
    }

    static bool is_shared_heap_allocation_type(const std::shared_ptr<const Type>& type) {
        return type == getHeapSharedAllocationType();
    }

    bool is_uncopyable(const std::shared_ptr<const Type>& type) const {
        return registered_unique_impl_for(type) != nullptr ||
            (has_drop(type) && !is_shared_heap_allocation_type(type));
    }

    bool is_terminal_handoff(const std::shared_ptr<Binding>& binding, size_t depth) const {
        return terminal_handoff_.has_value() &&
            terminal_handoff_->depth == depth &&
            terminal_handoff_->binding == binding;
    }

    void drop_value(const Value& value, const token& diag) {
        const Value* impl = registered_drop_impl_for(value.getType());
        if (impl == nullptr) {
            return;
        }
        if (!impl->isStructInstance()) {
            fail(diag, "`drop implementation must be a Dropness instance");
        }

        const auto& fields = *impl->asStructInstance().fields;
        auto found = fields.find("drop");
        if (found == fields.end()) {
            fail(diag, "`drop implementation is missing method 'drop'");
        }

        Value result = call_callable_with_values(found->second, {value}, diag, {false}, {false});
        if (!result.isVoid()) {
            fail(diag, "`drop.drop must return void");
        }
    }

    void destroy_heap_allocation(const Value& value, const token& diag) {
        if (!value.isHeapAllocation() || value.getType() != getHeapAllocationType()) {
            fail(diag, "`heap_destroy expects a heap allocation");
        }

        const auto& state = value.asHeapAllocation().state;
        if (!state) {
            fail(diag, "Invalid heap allocation");
        }
        if (state->destroyed) {
            return;
        }

        Value stored = state->stored ? *state->stored : VoidVal();
        state->destroyed = true;
        state->stored.reset();
        drop_value(stored, diag);
    }

    void retain_shared_heap_allocation(const Value& value, const token& diag) {
        if (!value.isHeapAllocation() || value.getType() != getHeapSharedAllocationType()) {
            return;
        }

        const auto& state = value.asHeapAllocation().state;
        if (!state || state->destroyed || !state->stored) {
            fail(diag, "Cannot retain destroyed shared heap allocation");
        }
        ++state->strong_count;
    }

    void release_shared_heap_allocation(const Value& value, const token& diag) {
        if (!value.isHeapAllocation() || value.getType() != getHeapSharedAllocationType()) {
            fail(diag, "`heap_shared_destroy expects a shared heap allocation");
        }

        const auto& state = value.asHeapAllocation().state;
        if (!state) {
            fail(diag, "Invalid shared heap allocation");
        }
        if (state->destroyed) {
            return;
        }

        if (state->strong_count > 0) {
            --state->strong_count;
        }
        if (state->strong_count != 0) {
            return;
        }

        Value stored = state->stored ? *state->stored : VoidVal();
        state->destroyed = true;
        state->stored.reset();
        drop_value(stored, diag);
    }

    void disown_shared_heap_allocation_without_destroy(const Value& value, const token& diag) {
        if (!value.isHeapAllocation() || value.getType() != getHeapSharedAllocationType()) {
            return;
        }

        const auto& state = value.asHeapAllocation().state;
        if (!state || state->destroyed || !state->stored) {
            fail(diag, "Cannot transfer destroyed shared heap allocation");
        }
        if (state->strong_count == 0) {
            fail(diag, "Cannot transfer unowned shared heap allocation");
        }
        --state->strong_count;
    }

    void retain_owned_value(const Value& value, const token& diag) {
        retain_shared_heap_allocation(value, diag);
    }

    void leave_scope(std::shared_ptr<Binding> handoff_binding, const token& diag) {
        auto scope = scopes_.back();
        std::vector<Value> values_to_drop;

        for (auto it = scope->declaration_order.rbegin(); it != scope->declaration_order.rend(); ++it) {
            const auto& binding = *it;
            if (binding == handoff_binding || !binding->ownsCV()) {
                continue;
            }

            const Value& cv = binding->getCV();
            if (!has_drop(cv.getType())) {
                continue;
            }

            values_to_drop.push_back(cv);
            binding->setOwnsCV(false);
        }

        scopes_.pop_back();

        for (const Value& value : values_to_drop) {
            drop_value(value, diag);
        }
    }

    bool has_setness(const Value& value) const {
        return registered_setness_impl_for(value.getType()) != nullptr ||
            value.getType()->hasSetness();
    }

    Value call_setness_bp(const Value& set, const Value& value, const token& diag) {
        if (const Value* impl = registered_setness_impl_for(set.getType())) {
            if (!impl->isStructInstance()) {
                fail(diag, "`Set implementation must be a struct instance");
            }
            const auto& fields = *impl->asStructInstance().fields;
            auto found = fields.find("belongs");
            if (found == fields.end()) {
                fail(diag, "`Set implementation is missing method 'belongs'");
            }
            return call_callable_with_values(found->second, {set, value}, diag, {false, false}, {false, true});
        }

        if (!set.getType()->hasSetness()) {
            fail(diag, "Type '" + std::string(set.getType()->name()) + "' of value does not support set-ness");
        }
        return belongsTo(set, value);
    }

    Value call_setness_br(const Value& set, const Value& lc, const token& diag) {
        if (const Value* impl = registered_setness_impl_for(set.getType())) {
            if (!impl->isStructInstance()) {
                fail(diag, "`Set implementation must be a struct instance");
            }
            const auto& fields = *impl->asStructInstance().fields;
            auto found = fields.find("belongs_range");
            if (found == fields.end()) {
                fail(diag, "`Set implementation is missing method 'belongs_range'");
            }
            return call_callable_with_values(found->second, {set, lc}, diag, {false, false}, {false, true});
        }

        if (!set.getType()->hasSetness()) {
            fail(diag, "Type '" + std::string(set.getType()->name()) + "' of value does not support set-ness");
        }
        return belongsRange(set, lc);
    }

    Value belongs_to(const Value& set, const Value& value, const token& diag) {
        if (set.isCompositeSet()) {
            const auto& comp = set.asCompositeSet();
            Value left_res = belongs_to(*comp.left, value, diag);
            if (!left_res.isBool()) {
                fail(diag, "Left operand of composite set did not return Bool for belonging");
            }
            if (comp.op == Value::CompositeSetOp::Union) {
                if (left_res.asBool()) return Value::make_bool(true);
                return belongs_to(*comp.right, value, diag);
            } else {
                if (!left_res.asBool()) {
                    return Value::make_bool(false);
                }
                Value right_res = belongs_to(*comp.right, value, diag);
                return right_res;
            }
        }

        if (set.isComplementSet()) {
            const auto& operand = set.asComplementSet().operand;
            if (!operand) {
                fail(diag, "Complement set operand is missing");
            }
            Value result = belongs_to(*operand, value, diag);
            if (!result.isBool()) {
                fail(diag, "Complement operand did not return Bool for belonging");
            }
            return Value::make_bool(!result.asBool());
        }

        if (set.isRange()) {
            auto range = set.asRange();
            if (value.getType() != range.start->getType()) {
                return Value::make_bool(false);
            }
            bool gte = value_compare_less_equal(*range.start, value, diag);
            bool lte = range.inclusive_end
                ? value_compare_less_equal(value, *range.end, diag)
                : value_compare_less(value, *range.end, diag);
            return Value::make_bool(gte && lte);
        }

        if (!set.isConstructedSet()) {
            return call_setness_bp(set, value, diag);
        }

        const auto& tag = set.asConstructedSetTag();
        const ConstructedSetExpr& expr = *tag.set;
        RuntimeScopeChain saved_scopes = std::move(scopes_);
        if (tag.captured_scopes) {
            scopes_ = *tag.captured_scopes;
        } else {
            scopes_ = saved_scopes;
        }

        bool predicate_scope_active = false;
        try {
            Value bound = expr.binding.type_bound ? evaluate(*expr.binding.type_bound) : VoidVal();
            if (expr.binding.type_bound) {
                Value in_bound = belongs_to(bound, value, expr.diagnostic_token);
                if (!in_bound.isBool()) {
                    fail(expr.diagnostic_token, "Set bound belonging predicate did not return Bool");
                }
                if (!in_bound.asBool()) {
                    scopes_ = std::move(saved_scopes);
                    return Value::make_bool(false);
                }
            }

            scopes_.push_back(std::make_shared<Scope>());
            predicate_scope_active = true;
            auto binding = std::make_shared<Binding>(bound, bound, value, expr.binding.is_final);
            define_binding(expr.binding.name.lexeme, std::move(binding), expr.binding.name);

            Value predicate_result = evaluate(*expr.condition);
            if (!predicate_result.isBool()) {
                fail(expr.diagnostic_token, "Constructed set predicate must evaluate to Bool");
            }

            scopes_.pop_back();
            predicate_scope_active = false;
            scopes_ = std::move(saved_scopes);
            return predicate_result;
        } catch (...) {
            if (predicate_scope_active) {
                scopes_.pop_back();
            }
            scopes_ = std::move(saved_scopes);
            throw;
        }
    }

    void require_set_operand(const Value& value, const token& diag) const {
        if (!has_setness(value)) {
            fail(diag, "Expected set operand, got " + value.toString());
        }
    }

    static bool contains_value(const std::vector<Value>& values, const Value& value) {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    static void append_unique(std::vector<Value>& values, const Value& value) {
        if (!contains_value(values, value)) {
            values.push_back(value);
        }
    }

    std::vector<Value> finite_elements(const Value& set, const token& diag) {
        require_set_operand(set, diag);

        if (set.isEnumeratedSet()) {
            std::vector<Value> elements;
            for (const auto& element : set.asEnumeratedSet()) {
                append_unique(elements, element);
            }
            return elements;
        }

        if (set.isRange()) {
            std::vector<Value> elements;
            auto range = set.asRange();
            Value current = *range.start;
            int64_t iterations = 0;
            auto in_range = [&]() {
                return range.inclusive_end
                    ? value_compare_less_equal(current, *range.end, diag)
                    : value_compare_less(current, *range.end, diag);
            };

            while (in_range()) {
                if (iterations++ >= MAX_LOOP_ITERATIONS) {
                    fail(diag, "Finite set materialization limit exceeded");
                }
                append_unique(elements, current);

                if (current.isInt()) {
                    current = Value::make_int(current.asInt() + BigInt(1));
                } else if (current.isChar()) {
                    current = Value::make_char(current.asChar() + 1);
                } else {
                    fail(diag, "Type is not materializable in range");
                }
            }
            return elements;
        }

        if (set.isConstructedSet()) {
            const auto& tag = set.asConstructedSetTag();
            const ConstructedSetExpr& expr = *tag.set;
            RuntimeScopeChain saved_scopes = std::move(scopes_);
            if (tag.captured_scopes) {
                scopes_ = *tag.captured_scopes;
            } else {
                scopes_ = saved_scopes;
            }

            std::vector<Value> candidates;
            try {
                if (!expr.binding.type_bound) {
                    fail(diag, "Cannot materialize unbounded constructed set");
                }

                Value bound = evaluate(*expr.binding.type_bound);
                candidates = finite_elements(bound, expr.diagnostic_token);
                scopes_ = std::move(saved_scopes);
            } catch (...) {
                scopes_ = std::move(saved_scopes);
                throw;
            }

            std::vector<Value> elements;
            for (const auto& candidate : candidates) {
                if (as_bool(belongs_to(set, candidate, expr.diagnostic_token), expr.diagnostic_token)) {
                    append_unique(elements, candidate);
                }
            }
            return elements;
        }

        if (set.isCompositeSet()) {
            const auto& comp = set.asCompositeSet();
            if (comp.op == Value::CompositeSetOp::Union) {
                std::vector<Value> elements = finite_elements(*comp.left, diag);
                for (const auto& element : finite_elements(*comp.right, diag)) {
                    append_unique(elements, element);
                }
                return elements;
            } else { // Intersection
                std::vector<Value> base_elements;
                const Value* other = nullptr;
                try {
                    base_elements = finite_elements(*comp.left, diag);
                    other = comp.right.get();
                } catch (const std::exception&) {
                    base_elements = finite_elements(*comp.right, diag);
                    other = comp.left.get();
                }

                std::vector<Value> intersected;
                for (const auto& element : base_elements) {
                    if (as_bool(belongs_to(*other, element, diag), diag)) {
                        append_unique(intersected, element);
                    }
                }
                return intersected;
            }
        }

        if (set.isComplementSet()) {
            fail(diag, "Set operator requires a finite enumerable set");
        }

        if (auto b = lookup_binding_optional("`empty")) {
            if (set == b->getCV()) {
                return {};
            }
        }

        if (set.isBool() || set.isInt() || set.isString() || set.isSymbol() || set.isMinted()) {
            return {set};
        }

        if (set.isType() && set.asType() == getBoolType()) {
            return {True(), False()};
        }

        fail(diag, "Set operator requires a finite enumerable set");
    }

    Value set_union(const Value& left, const Value& right, const token& diag) {
        require_set_operand(left, diag);
        require_set_operand(right, diag);
        return Value::make_composite_set(left, right, Value::CompositeSetOp::Union);
    }

    Value set_intersection(const Value& left, const Value& right, const token& diag) {
        require_set_operand(left, diag);
        require_set_operand(right, diag);
        return Value::make_composite_set(left, right, Value::CompositeSetOp::Intersection);
    }

    Value set_complement(const Value& operand, const token& diag) {
        require_set_operand(operand, diag);
        return Value::make_complement_set(operand);
    }



    void enforce_constraint(const Value& constraint, const Value& value, const token& diag) {
        if (constraint.isVoid()) {
            return;
        }
        Value belongs = belongs_to(constraint, value, diag);
        if (!belongs.isBool()) {
            fail(diag, "Constraint belonging predicate did not return Bool");
        }
        if (!belongs.asBool()) {
            fail(diag, "Constraint violation: " + display_string(value) + " does not belong to " + constraint.toString());
        }
    }

    Value builtin_identifier(std::string_view name, const token& diag) const {
        auto [binding, depth] = lookup_binding_with_depth(name, diag);
        Value cv = binding->getCV();
        
        if (binding->ownsCV() && is_uncopyable(cv.getType())) {
            if (!is_terminal_handoff(binding, depth)) {
                fail(diag, "Cannot copy unique or droppable value; use terminal move semantics (e.g., break)");
            }
        }
        
        return cv;
    }

    Value borrow_identifier(std::string_view name, const token& diag) const {
        return lookup_binding(name, diag)->getCV();
    }

    Value evaluate_deref_target(const Expr& expr) {
        if (const auto* identifier = dynamic_cast<const IdentifierExpr*>(&expr)) {
            return borrow_identifier(identifier->name, identifier->diagnostic_token);
        }
        return evaluate(expr);
    }

    Value dereference_heap_allocation(const Value& value, const token& diag) const {
        if (!value.isHeapAllocation()) {
            fail(diag, "Cannot dereference non-heap allocation value");
        }

        const auto& state = value.asHeapAllocation().state;
        if (!state || state->destroyed || !state->stored) {
            fail(diag, "Cannot dereference destroyed heap allocation");
        }
        return *state->stored;
    }

    Value builtin_intrinsic(std::string_view name, const token& diag) const {
        if (is_name(name, "`import")) {
            fail(diag, "`import is only available as a call");
        }
        return lookup_binding(name, diag)->getCV();
    }

    Value boot_bind(std::string_view name, const token& diag) const {
        if (is_name(name, "io.print")) return Value::make_host_function(Value::HostFunction::Print);
        if (is_name(name, "io.write")) return Value::make_host_function(Value::HostFunction::Write);
        if (is_name(name, "io.input")) return Value::make_host_function(Value::HostFunction::Input);
        if (is_name(name, "testing.inject_stdin")) return Value::make_host_function(Value::HostFunction::InjectStdin);
        if (is_name(name, "compute.invoke")) return Value::make_host_function(Value::HostFunction::Invoke);
        if (is_name(name, "compute.function_args")) return Value::make_host_function(Value::HostFunction::FunctionArgs);
        if (is_name(name, "nature.nature_of") || is_name(name, "natures.nature_of") || is_name(name, "types.type_of")) return Value::make_host_function(Value::HostFunction::TypeOf);
        if (is_name(name, "values.same")) return Value::make_host_function(Value::HostFunction::Same);
        if (is_name(name, "system.exit")) return Value::make_host_function(Value::HostFunction::Exit);

        if (is_name(name, "natures.mint_finite") || is_name(name, "types.mint_finite")) return Value::make_host_function(Value::HostFunction::MintFinite);
        if (is_name(name, "natures.is_struct_nature") || is_name(name, "types.is_struct_type")) return Value::make_host_function(Value::HostFunction::IsStructType);

        if (is_name(name, "traits.make")) return Value::make_host_function(Value::HostFunction::MakeTrait);
        if (is_name(name, "traits.interface")) return Value::make_host_function(Value::HostFunction::Interface);
        if (is_name(name, "traits.implements")) return Value::make_host_function(Value::HostFunction::Implements);
        if (is_name(name, "traits.implementation")) return Value::make_host_function(Value::HostFunction::Implementation);
        if (is_name(name, "traits.implement")) return Value::make_host_function(Value::HostFunction::Implement);
        if (is_name(name, "testing.expect")) return Value::make_host_function(Value::HostFunction::Expect);
        if (is_name(name, "testing.expect_stdout")) return Value::make_host_function(Value::HostFunction::ExpectStdout);
        if (is_name(name, "testing.expect_stderr")) return Value::make_host_function(Value::HostFunction::ExpectStderr);
        if (is_name(name, "testing.expect_exit")) return Value::make_host_function(Value::HostFunction::ExpectExit);
        if (is_name(name, "testing.expect_test_failure")) return Value::make_host_function(Value::HostFunction::ExpectTestFailure);

        if (is_name(name, "values.undecided")) {
            if (const Value* v = get_registered_item("values.undecided")) return *v;
            fail(diag, "undecided_val intrinsic used before registry initialization");
        }
        if (is_name(name, "system.register")) return Value::make_host_function(Value::HostFunction::Register);
        if (is_name(name, "memory.heap_allocation")) return Value::make_type(getHeapAllocationType());
        if (is_name(name, "memory.heap_create")) return Value::make_host_function(Value::HostFunction::HeapCreate);
        if (is_name(name, "memory.heap_destroy")) return Value::make_host_function(Value::HostFunction::HeapDestroy);
        if (is_name(name, "memory.heap_shared_allocation")) return Value::make_type(getHeapSharedAllocationType());
        if (is_name(name, "memory.heap_shared_create")) return Value::make_host_function(Value::HostFunction::HeapSharedCreate);
        if (is_name(name, "memory.heap_shared_destroy")) return Value::make_host_function(Value::HostFunction::HeapSharedDestroy);
        if (is_name(name, "testing.enabled")) return Value::make_bool(testing_enabled_);
        if (is_name(name, "compute.is_pure")) return Value::make_host_function(Value::HostFunction::IsPure);
        if (is_name(name, "compute.lambda_param_space")) return Value::make_host_function(Value::HostFunction::LambdaParamSpace);
        if (is_name(name, "compute.lambda_result_space")) return Value::make_host_function(Value::HostFunction::LambdaResultSpace);
        if (is_name(name, "natures.construction_args") || is_name(name, "types.construction_args")) return Value::make_host_function(Value::HostFunction::ConstructionArgs);
        if (is_name(name, "natures.construct") || is_name(name, "types.construct")) return Value::make_host_function(Value::HostFunction::Construct);
        if (is_name(name, "sets.natures_in_set") || is_name(name, "sets.types_in_set")) return Value::make_host_function(Value::HostFunction::TypesInSet);
        if (is_name(name, "sets.enumerable")) return Value::make_host_function(Value::HostFunction::IsEnumerable);
        if (is_name(name, "sets.coextensive")) return Value::make_host_function(Value::HostFunction::Coextensive);
        fail(diag, "Unknown boot binding '" + to_key(name) + "'");
    }

    static bool is_explicit_module_key(const std::string& key) {
        return key.rfind("./", 0) == 0 ||
            key.rfind("../", 0) == 0 ||
            key.rfind("/", 0) == 0;
    }

    static std::filesystem::path with_chirp_extension(std::filesystem::path path) {
        if (path.extension().empty()) {
            path.replace_extension(".chirp");
        }
        return path;
    }

    static std::filesystem::path normalized_absolute_path(const std::filesystem::path& path) {
        return std::filesystem::absolute(path).lexically_normal();
    }

    std::filesystem::path current_source_dir() const {
        if (source_stack_.empty() || source_stack_.back().empty()) {
            return std::filesystem::current_path();
        }

        std::filesystem::path source_path(source_stack_.back());
        if (source_path.filename().string().rfind("<", 0) == 0) {
            return std::filesystem::current_path();
        }
        return normalized_absolute_path(source_path).parent_path();
    }

    std::filesystem::path resolve_chirp_module_key(const std::string& key, const token& diag) const {
        if (is_explicit_module_key(key)) {
            std::filesystem::path path(key);
            if (path.is_relative()) {
                path = current_source_dir() / path;
            }
            return normalized_absolute_path(with_chirp_extension(path));
        }

        if (key.rfind("std/", 0) == 0) {
            if (!chirp_root_.has_value()) {
                fail(diag, "Cannot resolve std import without an active Chirp root");
            }
            std::filesystem::path relative(key.substr(4));
            return normalized_absolute_path(with_chirp_extension(*chirp_root_ / "std" / relative));
        }

        fail(diag, "Logical module imports outside 'std/' are not supported yet: " + key);
    }

    static std::string read_source_file(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    Value evaluate_chirp_module(const std::filesystem::path& path, const token& diag) {
        std::string identity = path.generic_string();
        auto found = module_cache_.find(identity);
        if (found != module_cache_.end()) {
            if (found->second.state == ModuleCacheEntry::State::Loaded) {
                return found->second.value;
            }
            if (found->second.state == ModuleCacheEntry::State::Failed) {
                throw std::runtime_error(found->second.error);
            }
            fail(diag, "Import cycle detected for module '" + identity + "'");
        }

        auto [inserted_it, _] = module_cache_.emplace(identity, ModuleCacheEntry{});
        ModuleCacheEntry& entry = inserted_it->second;
        entry.state = ModuleCacheEntry::State::Loading;

        try {
            auto unit = std::make_unique<SourceUnit>();
            unit->label = path.string();
            unit->source = read_source_file(path);
            auto tokens = frontend::tokenize(unit->source);
            unit->stmts = frontend::parse(tokens);

            SourceUnit* loaded = unit.get();
            module_sources_.push_back(std::move(unit));

            RuntimeScopeChain saved_scopes = std::move(scopes_);
            auto module_scope = std::make_shared<Scope>();
            scopes_ = {saved_scopes.front(), module_scope};

            std::map<std::string, std::shared_ptr<Binding>> exports;
            source_stack_.push_back(path.string());
            module_export_stack_.push_back(&exports);
            try {
                execute_statements(loaded->stmts);
                module_export_stack_.pop_back();
                source_stack_.pop_back();
                scopes_ = std::move(saved_scopes);
            } catch (...) {
                module_export_stack_.pop_back();
                source_stack_.pop_back();
                scopes_ = std::move(saved_scopes);
                throw;
            }

            entry.value = Value::make_module(identity, std::move(exports));
            entry.state = ModuleCacheEntry::State::Loaded;
            return entry.value;
        } catch (const ScriptExit&) {
            module_cache_.erase(identity);
            throw;
        } catch (const std::exception& e) {
            entry.error = path.string() + ": " + e.what();
            entry.state = ModuleCacheEntry::State::Failed;
            throw std::runtime_error(entry.error);
        }
    }

    Value call_import(const std::vector<Argument>& args, const token& diag) {
        if (args.empty() || args.size() > 2) {
            fail(diag, "`import expects one or two positional arguments: key and optional format");
        }
        for (const auto& arg : args) {
            if (arg.name.has_value()) {
                fail(diag, "`import does not support named arguments");
            }
        }
        Value name = evaluate(*args[0].value);
        if (!name.isString()) {
            fail(diag, "`import expects first argument (key) to be a string");
        }

        std::string format_text = "chirp";
        if (args.size() == 2) {
            Value format = evaluate(*args[1].value);
            if (!format.isString()) {
                fail(diag, "`import expects second argument (format) to be a string");
            }
            format_text = format.asString();
        }

        if (format_text == "chirp") {
            std::filesystem::path path = resolve_chirp_module_key(name.asString(), diag);
            return evaluate_chirp_module(path, diag);
        }

        if (format_text != "__chirp_boot") {
            fail(diag, "Unsupported `import format \"" + format_text + "\"");
        }
        if (!boot_mode_) {
            fail(diag, "`import with format \"__chirp_boot\" is only available while evaluating boot files");
        }
        return boot_bind(name.asString(), diag);
    }

    Value call_host_function(Value::HostFunction fn, const std::vector<Argument>& args, const token& diag) {
        switch (fn) {
            case Value::HostFunction::Register: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`register expects two positional arguments");
                }
                Value key_val = evaluate(*args[0].value);
                if (!key_val.isString()) {
                    fail(diag, "First argument to `register must be a string");
                }
                Value value = evaluate(*args[1].value);
                registered_items_[key_val.asString()] = value;
                return VoidVal();
            }
            case Value::HostFunction::Print: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`print expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                out_ << display_string(value) << '\n';
                return VoidVal();
            }
            case Value::HostFunction::Write: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`write expects two positional arguments");
                }
                Value what_val = evaluate(*args[0].value);
                Value to_val = evaluate(*args[1].value);

                if (!to_val.isInt()) {
                    fail(diag, "`write expects 'to' to be an integer file descriptor");
                }

                BigInt fd = to_val.asInt();
                if (fd == BigInt(1)) {
                    out_ << display_string(what_val);
                } else if (fd == BigInt(2)) {
                    std::cerr << display_string(what_val);
                } else {
                    fail(diag, "Unsupported file descriptor for `write");
                }
                return VoidVal();
            }
            case Value::HostFunction::Input: {
                if (!args.empty()) {
                    fail(diag, "`input expects zero arguments");
                }
                if (stdin_injected_) {
                    if (stdin_cursor_ < injected_stdin_.size()) {
                        size_t found = injected_stdin_.find('\n', stdin_cursor_);
                        if (found != std::string::npos) {
                            std::string line = injected_stdin_.substr(stdin_cursor_, found - stdin_cursor_);
                            stdin_cursor_ = found + 1;
                            return Value::make_string(line);
                        } else {
                            std::string line = injected_stdin_.substr(stdin_cursor_);
                            stdin_cursor_ = injected_stdin_.size();
                            return Value::make_string(line);
                        }
                    } else {
                        return Value::make_string("");
                    }
                } else {
                    std::string line;
                    if (!std::getline(std::cin, line)) {
                        return Value::make_string("");
                    }
                    return Value::make_string(line);
                }
            }
            case Value::HostFunction::InjectStdin: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`inject_stdin expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                if (!value.isString()) {
                    fail(diag, "`inject_stdin expects a string");
                }
                stdin_injected_ = true;
                injected_stdin_ += value.asString();
                return VoidVal();
            }
            case Value::HostFunction::Same: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`same expects two positional arguments");
                }
                Value left = evaluate(*args[0].value);
                Value right = evaluate(*args[1].value);
                return Value::make_bool(left == right);
            }
            case Value::HostFunction::Invoke: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`invoke_func expects two positional arguments: callee and args struct");
                }
                Value callee = evaluate(*args[0].value);
                Value args_val = evaluate(*args[1].value);

                if (callee.isLambda()) {
                    const auto& lambda_tag = callee.asLambdaTag();
                    if (!args_val.isStructInstance()) {
                        fail(diag, "`invoke_func second argument must be a struct");
                    }
                    const auto& fields = *args_val.asStructInstance().fields;
                    std::vector<Value> arg_values(lambda_tag.lambda->parameters.size());
                    for (size_t i = 0; i < lambda_tag.lambda->parameters.size(); ++i) {
                        std::string param_name = std::string(to_key(lambda_tag.lambda->parameters[i].name.lexeme));
                        auto it = fields.find(param_name);
                        if (it == fields.end()) {
                            fail(diag, "`invoke_func missing argument for parameter '" + param_name + "'");
                        }
                        arg_values[i] = it->second;
                    }
                    return call_lambda_with_values(lambda_tag, std::move(arg_values), diag);
                } else {
                    fail(diag, "`invoke_func expects a lambda");
                }
            }
            case Value::HostFunction::FunctionArgs: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`function_args_func expects one positional argument");
                }
                Value callee = evaluate(*args.front().value);
                if (!callee.isLambda()) {
                    fail(diag, "`function_args_func expects a lambda");
                }
                return Value::make_type(lambda_param_struct_type(callee.asLambdaTag()));
            }
            case Value::HostFunction::TypeOf: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`type_of expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                return Value::make_type(value.getType());
            }
            case Value::HostFunction::Exit: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`exit expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                BigInt code_big = as_int(value, diag);
                if (code_big < BigInt(0) || code_big > BigInt(255)) {
                    fail(diag, "`exit expects an integer exit code between 0 and 255");
                }
                throw ScriptExit(static_cast<int>(code_big.to_int64()));
            }
            case Value::HostFunction::Mint: {
                if (!args.empty()) {
                    fail(diag, "`mint expects no arguments");
                }
                uint64_t id = next_mint_id_++;
                auto type = std::make_shared<MintedType>(id);
                return Value::make_minted(std::move(type), id);
            }
            case Value::HostFunction::MintFinite: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`mint_finite expects one positional integer argument");
                }
                Value count_value = evaluate(*args.front().value);
                BigInt count_big = as_int(count_value, diag);
                if (!count_big.fits_int64()) {
                    fail(diag, "`mint_finite count is out of range");
                }
                int64_t count = count_big.to_int64();
                if (count < 0) {
                    fail(diag, "`mint_finite count must be non-negative");
                }

                uint64_t type_id = next_mint_id_++;
                auto minted_type = std::make_shared<MintedType>(type_id);
                std::vector<Value> values;
                values.reserve(static_cast<size_t>(count));
                for (int64_t i = 0; i < count; ++i) {
                    values.push_back(Value::make_minted(minted_type, next_mint_id_++));
                }

                std::vector<OrderedStructFieldSpec> fields = {
                    OrderedStructFieldSpec{"nature"},
                    OrderedStructFieldSpec{"values"}
                };
                auto result_type = std::make_shared<RuntimeStructType>(std::move(fields));
                return Value::make_struct_instance(
                    std::move(result_type),
                    {
                        {"nature", Value::make_type(minted_type)},
                        {"values", Value::make_list(std::move(values))}
                    }
                );
            }
            case Value::HostFunction::IsStructType: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`is_struct_type expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                if (!value.isType()) {
                    return Value::make_bool(false);
                }
                return Value::make_bool(is_struct_type(value.asType()));
            }
            case Value::HostFunction::Trait: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`trait expects one positional argument");
                }
                Value interface = evaluate(*args.front().value);
                if (!interface.isVoid() && !has_setness(interface)) {
                    fail(diag, "`trait interface must be a set");
                }
                return Value::make_trait(next_trait_id_++, std::move(interface));
            }
            case Value::HostFunction::MakeTrait: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`make_trait expects one positional argument");
                }
                Value interface = evaluate(*args.front().value);
                if (!interface.isVoid() &&
                    (!interface.isType() || !is_struct_type(interface.asType()))) {
                    fail(diag, "`make_trait expects either void or a struct type");
                }
                return Value::make_trait(next_trait_id_++, std::move(interface));
            }
            case Value::HostFunction::Interface: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`interface expects one positional argument");
                }
                Value trait = evaluate(*args.front().value);
                if (trait.isTrait()) {
                    return trait.asTraitInterface();
                }
                fail(diag, "`interface expects a trait");
            }
            case Value::HostFunction::Implements: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`implements expects two positional arguments");
                }
                Value trait = evaluate(*args[0].value);
                Value on = evaluate(*args[1].value);
                if (!trait.isTrait()) {
                    fail(diag, "`implements first argument must be a trait");
                }
                if (!on.isType()) {
                    fail(diag, "`implements second argument must be a type");
                }
                return Value::make_bool(registered_impl_for(trait, on.asType()) != nullptr);
            }
            case Value::HostFunction::Implementation: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`implementation expects two positional arguments");
                }
                Value trait = evaluate(*args[0].value);
                Value on = evaluate(*args[1].value);
                if (!trait.isTrait()) {
                    fail(diag, "`implementation first argument must be a trait");
                }
                if (!on.isType()) {
                    fail(diag, "`implementation second argument must be a type");
                }
                const Value* impl = registered_impl_for(trait, on.asType());
                if (impl == nullptr) {
                    fail(diag, "No implementation registered for trait/type pair");
                }
                return *impl;
            }
            case Value::HostFunction::Implement: {
                const Argument* trait_arg = nullptr;
                const Argument* on_arg = nullptr;
                const Argument* impl_arg = nullptr;
                for (const auto& arg : args) {
                    if (!arg.name.has_value()) {
                        fail(diag, "`implement expects named arguments");
                    }

                    std::string name = to_key(arg.name->lexeme);
                    if (name == "trait") {
                        if (trait_arg != nullptr) {
                            fail(*arg.name, "Duplicate argument for parameter 'trait'");
                        }
                        trait_arg = &arg;
                    } else if (name == "on") {
                        if (on_arg != nullptr) {
                            fail(*arg.name, "Duplicate argument for parameter 'on'");
                        }
                        on_arg = &arg;
                    } else if (name == "impl") {
                        if (impl_arg != nullptr) {
                            fail(*arg.name, "Duplicate argument for parameter 'impl'");
                        }
                        impl_arg = &arg;
                    } else {
                        fail(*arg.name, "Unknown `implement parameter '" + name + "'");
                    }
                }

                if (trait_arg == nullptr) {
                    fail(diag, "Missing argument for parameter 'trait'");
                }
                if (on_arg == nullptr) {
                    fail(diag, "Missing argument for parameter 'on'");
                }
                if (impl_arg == nullptr) {
                    fail(diag, "Missing argument for parameter 'impl'");
                }

                Value trait = evaluate(*trait_arg->value);
                Value on = evaluate(*on_arg->value);
                if (!trait.isTrait()) {
                    fail(*trait_arg->name, "`implement trait must be a trait");
                }
                if (!on.isType()) {
                    fail(*on_arg->name, "`implement on must be a type value");
                }

                Value interface = trait.asTraitInterface();
                if (!interface.isVoid() &&
                    (!interface.isType() || !is_struct_type(interface.asType()))) {
                    fail(*trait_arg->name, "`implement trait interface must be either void or a struct type");
                }
                Value impl = evaluate_with_expected_type(*impl_arg->value, interface, *impl_arg->name);
                if (interface.isVoid() && !impl.isVoid()) {
                    fail(*impl_arg->name, "Marker trait implementations must be void");
                }
                enforce_constraint(interface, impl, *impl_arg->name);

                if (registered_impl_for(trait, on.asType()) != nullptr) {
                    fail(*on_arg->name, "Duplicate implementation for trait/on pair");
                }
                implementations_.push_back(Implementation{
                    std::move(trait),
                    on.asType(),
                    std::move(impl)
                });
                return VoidVal();
            }
            case Value::HostFunction::Expect: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`expect expects one positional argument");
                }
                expectations.has_expectations = true;
                expectations.expectation_checks += 1;
                Value value = evaluate(*args.front().value);
                if (!value.isBool()) {
                    fail(diag, "`expect expects a Bool expression, got " + value.toString());
                }
                if (!value.asBool()) {
                    fail(diag, "`expect check failed");
                }
                return VoidVal();
            }
            case Value::HostFunction::ExpectStdout: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`expect_stdout expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                if (!value.isString()) {
                    fail(diag, "`expect_stdout expects a string");
                }
                expectations.has_expectations = true;
                if (!expectations.expected_stdout.has_value()) {
                    expectations.expected_stdout = value.asString();
                } else {
                    *expectations.expected_stdout += value.asString();
                }
                return VoidVal();
            }
            case Value::HostFunction::ExpectStderr: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`expect_stderr expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                if (!value.isString()) {
                    fail(diag, "`expect_stderr expects a string");
                }
                expectations.has_expectations = true;
                if (!expectations.expected_stderr.has_value()) {
                    expectations.expected_stderr = value.asString();
                } else {
                    *expectations.expected_stderr += value.asString();
                }
                return VoidVal();
            }
            case Value::HostFunction::ExpectExit: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`expect_exit expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                BigInt code_big = as_int(value, diag);
                if (code_big < BigInt(0) || code_big > BigInt(255)) {
                    fail(diag, "`expect_exit expects an integer exit code between 0 and 255");
                }
                expectations.has_expectations = true;
                expectations.expected_exit = static_cast<int>(code_big.to_int64());
                return VoidVal();
            }
            case Value::HostFunction::ExpectTestFailure: {
                if (!args.empty()) {
                    fail(diag, "`expect_test_failure expects no arguments");
                }
                expectations.has_expectations = true;
                expectations.expect_test_failure = true;
                return VoidVal();
            }
            case Value::HostFunction::IsPure: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`is_pure expects one positional argument");
                }
                Value arg_val = evaluate(*args.front().value);
                if (arg_val.isHostFunction()) {
                    return Value::make_bool(arg_val.asHostFunction() != Value::HostFunction::Print);
                }
                if (!arg_val.isLambda()) {
                    return Value::make_bool(false);
                }
                return Value::make_bool(check_purity(arg_val.asLambdaTag()));
            }
            case Value::HostFunction::LambdaParamSpace: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`lambda_param_space expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                if (!value.isLambda()) {
                    fail(diag, "`lambda_param_space expects a lambda");
                }
                return Value::make_type(lambda_param_struct_type(value.asLambdaTag()));
            }
            case Value::HostFunction::LambdaResultSpace: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`lambda_result_space expects two positional arguments");
                }
                Value value = evaluate(*args[0].value);
                if (!value.isLambda()) {
                    fail(diag, "`lambda_result_space expects a lambda");
                }
                (void)evaluate(*args[1].value);
                return lambda_result_space(value.asLambdaTag(), diag);
            }
            case Value::HostFunction::ConstructionArgs: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`construction_args expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                if (!value.isType() || !is_struct_type(value.asType())) {
                    fail(diag, "`construction_args expects a struct type");
                }
                return Value::make_type(construction_arg_struct_type(value.asType(), diag));
            }
            case Value::HostFunction::Construct: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`construct expects two positional arguments");
                }
                Value type_value = evaluate(*args[0].value);
                Value params = evaluate(*args[1].value);
                if (!type_value.isType() || !is_struct_type(type_value.asType())) {
                    fail(diag, "`construct expects a struct type");
                }
                if (!params.isStructInstance()) {
                    fail(diag, "`construct expects a struct argument bundle");
                }
                return construct_struct_from_fields(type_value.asType(), *params.asStructInstance().fields, diag);
            }
            case Value::HostFunction::TypesInSet: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`types_in_set expects one positional argument");
                }
                Value set = evaluate(*args.front().value);
                std::vector<Value> types;
                try {
                    for (const auto& element : finite_elements(set, diag)) {
                        append_unique(types, Value::make_type(element.getType()));
                    }
                    return Value::make_enumerated_set(std::move(types));
                } catch (const std::exception&) {
                    if (set.isType()) {
                        return Value::make_enumerated_set({set});
                    }
                    if (set.isConstructedSet() && set.asConstructedSet().binding.type_bound) {
                        Value bound = evaluate(*set.asConstructedSet().binding.type_bound);
                        if (bound.isType()) {
                            return Value::make_enumerated_set({bound});
                        }
                    }
                    return type_set_value(diag);
                }
            }
            case Value::HostFunction::IsEnumerable: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`is_enumerable expects one positional argument");
                }
                Value set = evaluate(*args.front().value);
                try {
                    (void)finite_elements(set, diag);
                    return Value::make_bool(true);
                } catch (const std::exception&) {
                    return Value::make_bool(false);
                }
            }
            case Value::HostFunction::Coextensive: {
                if (args.size() != 2 || args[0].name.has_value() || args[1].name.has_value()) {
                    fail(diag, "`coextensive expects two positional arguments");
                }
                Value left = evaluate(*args[0].value);
                Value right = evaluate(*args[1].value);
                bool left_enumerable = false;
                bool right_enumerable = false;
                std::vector<Value> left_elements;
                std::vector<Value> right_elements;

                try {
                    left_elements = finite_elements(left, diag);
                    left_enumerable = true;
                } catch (const std::exception&) {
                }
                try {
                    right_elements = finite_elements(right, diag);
                    right_enumerable = true;
                } catch (const std::exception&) {
                }

                if (!left_enumerable && !right_enumerable) {
                    fail(diag, "`coextensive requires at least one finite enumerable operand");
                }

                const std::vector<Value>& base = left_enumerable ? left_elements : right_elements;
                const Value& other = left_enumerable ? right : left;
                for (const auto& element : base) {
                    if (!as_bool(belongs_to(other, element, diag), diag)) {
                        return Value::make_bool(false);
                    }
                }

                if (left_enumerable && right_enumerable) {
                    for (const auto& element : right_elements) {
                        if (!as_bool(belongs_to(left, element, diag), diag)) {
                            return Value::make_bool(false);
                        }
                    }
                }
                return Value::make_bool(true);
            }
            case Value::HostFunction::HeapCreate: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`heap_create expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                return Value::make_heap_allocation(next_heap_allocation_id_++, std::move(value));
            }
            case Value::HostFunction::HeapDestroy: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`heap_destroy expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                destroy_heap_allocation(value, diag);
                return VoidVal();
            }
            case Value::HostFunction::HeapSharedCreate: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`heap_shared_create expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                return Value::make_heap_shared_allocation(next_heap_allocation_id_++, std::move(value));
            }
            case Value::HostFunction::HeapSharedDestroy: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`heap_shared_destroy expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                release_shared_heap_allocation(value, diag);
                return VoidVal();
            }
        }
        fail(diag, "Unknown host function");
    }

    BigInt binary_int(const Value& left, const Value& right, BinaryOp op, const token& diag) {
        BigInt a = as_int(left, diag);
        BigInt b = as_int(right, diag);
        if ((op == BinaryOp::Div || op == BinaryOp::Mod) && b == BigInt(0)) {
            if (op == BinaryOp::Div) {
                fail(diag, "Division by zero");
            } else {
                fail(diag, "Modulo by zero");
            }
        }
        try {
            switch (op) {
                case BinaryOp::Add: return a + b;
                case BinaryOp::Sub: return a - b;
                case BinaryOp::Mul: return a * b;
                case BinaryOp::Div: return a / b;
                case BinaryOp::Mod: return a % b;
                default:
                    fail(diag, "Unsupported integer operator");
            }
        } catch (const std::out_of_range& e) {
            fail(diag, e.what());
        }
    }

    Value call_lambda_with_values(
        const Value::LambdaTag& lambda_tag,
        std::vector<Value> arg_values,
        const token& diag,
        std::vector<bool> arg_ownership = {},
        std::vector<bool> enforce_constraints_mask = {}) {
        const LambdaExpr& lambda = *lambda_tag.lambda;
        if (arg_values.size() != lambda.parameters.size()) {
            fail(diag, "Function expected " + std::to_string(lambda.parameters.size()) +
                " arguments, got " + std::to_string(arg_values.size()));
        }
        if (!arg_ownership.empty() && arg_ownership.size() != arg_values.size()) {
            fail(diag, "Internal error: argument ownership metadata does not match argument count");
        }
        if (!enforce_constraints_mask.empty() && enforce_constraints_mask.size() != arg_values.size()) {
            fail(diag, "Internal error: argument constraint metadata does not match argument count");
        }

        RuntimeScopeChain saved_scopes = std::move(scopes_);
        if (lambda_tag.captured_scopes) {
            scopes_ = *lambda_tag.captured_scopes;
        } else {
            scopes_ = saved_scopes;
        }
        scopes_.push_back(std::make_shared<Scope>());
        try {
            for (size_t i = 0; i < lambda.parameters.size(); ++i) {
                const NamedBinding& param = lambda.parameters[i];
                Value constraint = param.type_bound ? evaluate(*param.type_bound) : VoidVal();
                bool enforce_arg_constraint = enforce_constraints_mask.empty() ? true : enforce_constraints_mask[i];
                if (enforce_arg_constraint) {
                    enforce_constraint(constraint, arg_values[i], param.name);
                }

                bool owns_arg = arg_ownership.empty() ? true : arg_ownership[i];
                if (owns_arg) {
                    retain_owned_value(arg_values[i], param.name);
                }
                auto binding = std::make_shared<Binding>(constraint, constraint, std::move(arg_values[i]), param.is_final, owns_arg);
                define_binding(param.name.lexeme, std::move(binding), param.name);
            }

            Value return_constraint = lambda.return_bound ? evaluate(*lambda.return_bound) : VoidVal();
            TerminalValue return_result = dynamic_cast<const AnonymousStructLiteralExpr*>(lambda.body.get()) != nullptr
                ? TerminalValue{evaluate_with_expected_type(*lambda.body, return_constraint, lambda.diagnostic_token), nullptr}
                : evaluate_terminal_expression(*lambda.body);
            Value return_value = std::move(return_result.value);
            enforce_constraint(return_constraint, return_value, lambda.diagnostic_token);

            if (return_result.handoff_binding) {
                disown_shared_heap_allocation_without_destroy(return_result.handoff_binding->getCV(), lambda.diagnostic_token);
                return_result.handoff_binding->setOwnsCV(false);
            }
            leave_scope(return_result.handoff_binding, lambda.diagnostic_token);
            scopes_ = std::move(saved_scopes);
            return return_value;
        } catch (...) {
            scopes_ = std::move(saved_scopes);
            throw;
        }
    }

    Value call_callable_with_values(
        const Value& callee,
        std::vector<Value> arg_values,
        const token& diag,
        std::vector<bool> arg_ownership = {},
        std::vector<bool> enforce_constraints_mask = {}) {
        if (!callee.isLambda()) {
            fail(diag, "Value is not callable: " + callee.toString());
        }
        return call_lambda_with_values(
            callee.asLambdaTag(),
            std::move(arg_values),
            diag,
            std::move(arg_ownership),
            std::move(enforce_constraints_mask));
    }

    bool is_struct_type(const std::shared_ptr<const Type>& type_ptr) const {
        return dynamic_cast<const StructType*>(type_ptr.get()) != nullptr ||
            dynamic_cast<const RuntimeStructType*>(type_ptr.get()) != nullptr;
    }

    std::shared_ptr<const Type> direct_struct_type_from_constraint(const Value& constraint) const {
        if (!constraint.isType()) {
            return nullptr;
        }
        std::shared_ptr<const Type> type_ptr = constraint.asType();
        if (!is_struct_type(type_ptr)) {
            return nullptr;
        }
        return type_ptr;
    }

    std::vector<OrderedStructFieldSpec> ordered_struct_fields(const std::shared_ptr<const Type>& type_ptr, const token& diag) const {
        if (const auto* struct_t = dynamic_cast<const StructType*>(type_ptr.get())) {
            std::vector<OrderedStructFieldSpec> fields;
            fields.reserve(struct_t->expr()->fields.size());
            for (const auto& field : struct_t->expr()->fields) {
                fields.push_back(OrderedStructFieldSpec{
                    std::string(field.name.lexeme),
                    field.is_mut,
                    field.is_final,
                    field.type_bound.get(),
                    field.initializer.get()
                });
            }
            return fields;
        }
        if (const auto* runtime_struct_t = dynamic_cast<const RuntimeStructType*>(type_ptr.get())) {
            return runtime_struct_t->fields();
        }
        fail(diag, "Expected a struct type");
        return {};
    }

    Value evaluate_with_expected_type(const Expr& expr, const Value& expected, const token& diag) {
        if (const auto* literal = dynamic_cast<const AnonymousStructLiteralExpr*>(&expr)) {
            std::shared_ptr<const Type> type_ptr = direct_struct_type_from_constraint(expected);
            if (type_ptr == nullptr) {
                fail(literal->diagnostic_token, "Anonymous struct literal requires a concrete struct context");
            }
            return call_struct_constructor(type_ptr, literal->fields, literal->diagnostic_token);
        }
        return evaluate(expr);
    }

    Value call_struct_constructor(std::shared_ptr<const Type> type_ptr, const std::vector<Argument>& args, const token& diag) {
        std::vector<OrderedStructFieldSpec> field_specs = ordered_struct_fields(type_ptr, diag);
        std::map<std::string, Value> fields;

        bool has_named = false;
        bool has_positional = false;
        for (const auto& arg : args) {
            has_named = has_named || arg.name.has_value();
            has_positional = has_positional || !arg.name.has_value();
        }

        if (has_named && has_positional) {
            fail(diag, "Cannot mix named and positional arguments");
        }
        if (has_positional && args.size() > field_specs.size()) {
            fail(diag, "Too many positional arguments for struct constructor");
        }

        std::map<std::string, const Argument*> provided_args;
        if (has_named) {
            for (const auto& arg : args) {
                std::string arg_name = std::string(arg.name->lexeme);
                if (provided_args.count(arg_name)) {
                    fail(diag, "Duplicate named argument: " + arg_name);
                }
                provided_args[arg_name] = &arg;
            }
        } else {
            for (size_t i = 0; i < args.size(); ++i) {
                provided_args[field_specs[i].name] = &args[i];
            }
        }

        for (const auto& pair : provided_args) {
            bool found = false;
            for (const auto& field : field_specs) {
                if (field.name == pair.first) { found = true; break; }
            }
            if (!found) {
                fail(diag, "Unknown field in struct constructor: " + pair.first);
            }
        }

        for (const auto& field : field_specs) {
            std::string field_name = field.name;
            Value constraint = field.type_bound ? evaluate(*field.type_bound) : VoidVal();
            Value field_val;
            if (provided_args.count(field_name)) {
                field_val = evaluate_with_expected_type(*provided_args[field_name]->value, constraint, *provided_args[field_name]->name);
            } else if (field.initializer) {
                field_val = evaluate_with_expected_type(*field.initializer, constraint, diag);
            } else {
                fail(diag, "Missing required field: " + field_name);
            }

            try {
                enforce_constraint(constraint, field_val, diag);
            } catch (const std::runtime_error& e) {
                fail(diag, "Struct field '" + field_name + "' constraint failure: " + std::string(e.what()));
            }

            fields[field_name] = std::move(field_val);
        }

        return Value::make_struct_instance(std::move(type_ptr), std::move(fields));
    }

    Value construct_struct_from_fields(
        std::shared_ptr<const Type> type_ptr,
        const std::map<std::string, Value>& provided_fields,
        const token& diag) {
        std::vector<OrderedStructFieldSpec> field_specs = ordered_struct_fields(type_ptr, diag);
        std::map<std::string, Value> fields;

        for (const auto& pair : provided_fields) {
            bool found = false;
            for (const auto& field : field_specs) {
                if (field.name == pair.first) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                fail(diag, "Unknown field in struct constructor: " + pair.first);
            }
        }

        for (const auto& field : field_specs) {
            std::string field_name = field.name;
            Value constraint = field.type_bound ? evaluate(*field.type_bound) : VoidVal();
            Value field_val;
            auto provided = provided_fields.find(field_name);
            if (provided != provided_fields.end()) {
                field_val = provided->second;
            } else if (field.initializer) {
                field_val = evaluate_with_expected_type(*field.initializer, constraint, diag);
            } else {
                fail(diag, "Missing required field: " + field_name);
            }

            try {
                enforce_constraint(constraint, field_val, diag);
            } catch (const std::runtime_error& e) {
                fail(diag, "Struct field '" + field_name + "' constraint failure: " + std::string(e.what()));
            }
            fields[field_name] = std::move(field_val);
        }

        return Value::make_struct_instance(std::move(type_ptr), std::move(fields));
    }

    Value evaluate_lambda_parameter_bound(const Value::LambdaTag& lambda_tag, const NamedBinding& param) {
        if (!param.type_bound) {
            return VoidVal();
        }

        RuntimeScopeChain saved_scopes = std::move(scopes_);
        if (lambda_tag.captured_scopes) {
            scopes_ = *lambda_tag.captured_scopes;
        } else {
            scopes_ = saved_scopes;
        }

        try {
            Value constraint = evaluate(*param.type_bound);
            scopes_ = std::move(saved_scopes);
            return constraint;
        } catch (...) {
            scopes_ = std::move(saved_scopes);
            throw;
        }
    }

    std::shared_ptr<const RuntimeStructType> lambda_param_struct_type(const Value::LambdaTag& lambda_tag) {
        auto found = lambda_param_struct_types_.find(lambda_tag.lambda);
        if (found != lambda_param_struct_types_.end()) {
            return found->second;
        }

        std::vector<OrderedStructFieldSpec> fields;
        fields.reserve(lambda_tag.lambda->parameters.size());
        for (const auto& param : lambda_tag.lambda->parameters) {
            fields.push_back(OrderedStructFieldSpec{
                std::string(param.name.lexeme),
                param.is_mut,
                param.is_final,
                param.type_bound.get(),
                nullptr
            });
        }

        auto type_ptr = std::make_shared<RuntimeStructType>(std::move(fields));
        lambda_param_struct_types_.emplace(lambda_tag.lambda, type_ptr);
        return type_ptr;
    }

    std::shared_ptr<const RuntimeStructType> construction_arg_struct_type(
        const std::shared_ptr<const Type>& type_ptr,
        const token& diag) {
        auto found = construction_arg_struct_types_.find(type_ptr.get());
        if (found != construction_arg_struct_types_.end()) {
            return found->second;
        }

        std::vector<OrderedStructFieldSpec> fields = ordered_struct_fields(type_ptr, diag);
        auto arg_type = std::make_shared<RuntimeStructType>(std::move(fields));
        construction_arg_struct_types_.emplace(type_ptr.get(), arg_type);
        return arg_type;
    }

    Value any_set_value(const token& diag) const {
        if (auto binding = lookup_binding_optional("`any")) {
            return binding->getCV();
        }
        fail(diag, "Bootstrap value `any is not available");
        return Value();
    }

    Value type_set_value(const token& diag) const {
        if (auto binding = lookup_binding_optional("`type")) {
            return binding->getCV();
        }
        fail(diag, "Bootstrap value `type is not available");
        return Value();
    }

    std::shared_ptr<const Type> reference_set_type(const token& diag) const {
        const Value* set_type = get_registered_item("natures.reference_set");
        if (!set_type) {
            set_type = get_registered_item("types.reference_set");
        }
        if (set_type) {
            if (set_type->isType() && is_struct_type(set_type->asType())) {
                return set_type->asType();
            }
            fail(diag, "Registered item 'reference_set_type' must be a struct type");
        }
        fail(diag, "Bootstrap reference set type is not available");
        return nullptr;
    }

    Value make_reference_set(Value target, bool is_mut, const token& diag) {
        std::map<std::string, Value> fields;
        fields.emplace("target", std::move(target));
        fields.emplace("mutable", Value::make_bool(is_mut));
        return construct_struct_from_fields(reference_set_type(diag), fields, diag);
    }

    Value lambda_result_space(const Value::LambdaTag& lambda_tag, const token& diag) {
        if (!lambda_tag.lambda->return_bound) {
            return any_set_value(diag);
        }

        RuntimeScopeChain saved_scopes = std::move(scopes_);
        if (lambda_tag.captured_scopes) {
            scopes_ = *lambda_tag.captured_scopes;
        } else {
            scopes_ = saved_scopes;
        }

        try {
            Value result = evaluate(*lambda_tag.lambda->return_bound);
            scopes_ = std::move(saved_scopes);
            return result;
        } catch (...) {
            scopes_ = std::move(saved_scopes);
            throw;
        }
    }

    Value lower_struct_arguments(
        const Value& receiver,
        const Value& impl,
        std::string_view space_method_name,
        const std::vector<Argument>& args,
        const token& diag) {
        if (!impl.isStructInstance()) {
            fail(diag, "Protocol implementation must be a struct instance");
        }

        const auto& fields = *impl.asStructInstance().fields;
        auto space_it = fields.find(std::string(space_method_name));
        if (space_it == fields.end()) {
            fail(diag, "Protocol implementation is missing method '" + std::string(space_method_name) + "'");
        }

        Value space = call_callable_with_values(space_it->second, {receiver}, diag, {false}, {false});
        if (!space.isType() || !is_struct_type(space.asType())) {
            fail(diag, std::string("Protocol method '") + std::string(space_method_name) + "' must return a struct type");
        }
        return call_struct_constructor(space.asType(), args, diag);
    }

    Value call_registered_callable(const Value& callee, const Value& impl, const std::vector<Argument>& args, const token& diag) {
        if (!impl.isStructInstance()) {
            fail(diag, "`Callable implementation must be a struct instance");
        }

        const auto& fields = *impl.asStructInstance().fields;
        auto invoke_it = fields.find("invoke");
        if (invoke_it == fields.end()) {
            fail(diag, "`Callable implementation is missing method 'invoke'");
        }

        Value lowered_args = lower_struct_arguments(callee, impl, "param_space", args, diag);
        return call_callable_with_values(invoke_it->second, {callee, lowered_args}, diag, {false, true}, {false, true});
    }

    Value call_lambda(const Value::LambdaTag& lambda_tag, const std::vector<Argument>& args, const token& diag) {
        const LambdaExpr& lambda = *lambda_tag.lambda;
        bool has_named = false;
        bool has_positional = false;
        for (const auto& arg : args) {
            has_named = has_named || arg.name.has_value();
            has_positional = has_positional || !arg.name.has_value();
        }

        if (has_named && has_positional) {
            fail(diag, "Cannot mix named and positional arguments");
        }

        std::vector<Value> arg_values(lambda.parameters.size());
        if (has_named) {
            std::unordered_map<std::string, size_t> parameter_indices;
            for (size_t i = 0; i < lambda.parameters.size(); ++i) {
                parameter_indices.emplace(to_key(lambda.parameters[i].name.lexeme), i);
            }

            std::vector<bool> provided(lambda.parameters.size(), false);
            for (const auto& arg : args) {
                std::string name = to_key(arg.name->lexeme);
                auto found = parameter_indices.find(name);
                if (found == parameter_indices.end()) {
                    fail(*arg.name, "Unknown parameter '" + name + "'");
                }

                size_t index = found->second;
                if (provided[index]) {
                    fail(*arg.name, "Duplicate argument for parameter '" + name + "'");
                }

                Value expected = dynamic_cast<const AnonymousStructLiteralExpr*>(arg.value.get()) != nullptr
                    ? evaluate_lambda_parameter_bound(lambda_tag, lambda.parameters[index])
                    : VoidVal();
                arg_values[index] = evaluate_with_expected_type(*arg.value, expected, *arg.name);
                provided[index] = true;
            }

            for (size_t i = 0; i < lambda.parameters.size(); ++i) {
                if (!provided[i]) {
                    fail(diag, "Missing argument for parameter '" + to_key(lambda.parameters[i].name.lexeme) + "'");
                }
            }
        } else {
            if (args.size() != lambda.parameters.size()) {
                fail(diag, "Function expected " + std::to_string(lambda.parameters.size()) +
                    " arguments, got " + std::to_string(args.size()));
            }

            for (size_t i = 0; i < args.size(); ++i) {
                Value expected = dynamic_cast<const AnonymousStructLiteralExpr*>(args[i].value.get()) != nullptr
                    ? evaluate_lambda_parameter_bound(lambda_tag, lambda.parameters[i])
                    : VoidVal();
                arg_values[i] = evaluate_with_expected_type(*args[i].value, expected, diag);
            }
        }

        return call_lambda_with_values(lambda_tag, std::move(arg_values), diag);
    }

    void bind_loop_iterator(const NamedBinding& binding, Value value, const token& diag) {
        Value constraint = binding.type_bound ? evaluate(*binding.type_bound) : VoidVal();
        enforce_constraint(constraint, value, diag);
        retain_owned_value(value, diag);

        auto iterator_binding = std::make_shared<Binding>(constraint, constraint, std::move(value), binding.is_final);
        define_binding(binding.name.lexeme, std::move(iterator_binding), binding.name);
    }

    void run_loop_body(const Expr& body) {
        evaluate(body);
    }

    void visit(const BinaryExpr& expr) override {
        if (expr.op == BinaryOp::Dot) {
            Value left = evaluate(*expr.left);
            auto* right_ident = dynamic_cast<const IdentifierExpr*>(expr.right.get());
            if (!right_ident) {
                fail(expr.diagnostic_token, "Right side of '.' must be an identifier");
            }
            if (left.isModule()) {
                const auto& exports = *left.asModule().exports;
                std::string export_name = std::string(right_ident->name);
                auto it = exports.find(export_name);
                if (it == exports.end()) {
                    fail(expr.diagnostic_token, "Module has no export '" + export_name + "'");
                }
                result_ = it->second->getCV();
                return;
            }
            if (left.isEnumFamily()) {
                std::string variant_name = std::string(right_ident->name);
                const auto& variants = left.asEnumFamily().variants;
                auto it = std::find(variants.begin(), variants.end(), variant_name);
                if (it == variants.end()) {
                    fail(expr.diagnostic_token, "Enum family has no variant '" + variant_name + "'");
                }
                size_t index = std::distance(variants.begin(), it);
                result_ = Value::make_enum_variant(left.asEnumFamily().node_id, variant_name, index);
                return;
            }
            if (!left.isStructInstance()) {
                fail(expr.diagnostic_token, "Left side of '.' must be a struct instance, module, or enum family");
            }
            const auto& fields = *left.asStructInstance().fields;
            std::string field_name = std::string(right_ident->name);
            auto it = fields.find(field_name);
            if (it == fields.end()) {
                fail(expr.diagnostic_token, "Struct has no field '" + field_name + "'");
            }
            result_ = it->second;
            return;
        }

        if (expr.op == BinaryOp::And) {
            bool left = as_bool(evaluate(*expr.left), expr.diagnostic_token);
            result_ = Value::make_bool(left && as_bool(evaluate(*expr.right), expr.diagnostic_token));
            return;
        }

        if (expr.op == BinaryOp::Or) {
            bool left = as_bool(evaluate(*expr.left), expr.diagnostic_token);
            result_ = Value::make_bool(left || as_bool(evaluate(*expr.right), expr.diagnostic_token));
            return;
        }

        Value left = evaluate(*expr.left);
        Value right = evaluate(*expr.right);

        switch (expr.op) {
            case BinaryOp::Add:
            case BinaryOp::Sub:
            case BinaryOp::Mul:
            case BinaryOp::Div:
            case BinaryOp::Mod:
                result_ = value_arithmetic(left, right, expr.op, expr.diagnostic_token);
                return;
            case BinaryOp::Eq:
                result_ = Value::make_bool(value_equality(left, right, expr.diagnostic_token));
                return;
            case BinaryOp::Neq:
                result_ = Value::make_bool(!value_equality(left, right, expr.diagnostic_token));
                return;
            case BinaryOp::Lt:
                result_ = Value::make_bool(value_compare_less(left, right, expr.diagnostic_token));
                return;
            case BinaryOp::Lte:
                result_ = Value::make_bool(value_compare_less_equal(left, right, expr.diagnostic_token));
                return;
            case BinaryOp::Gt:
                result_ = Value::make_bool(value_compare_less(right, left, expr.diagnostic_token));
                return;
            case BinaryOp::Gte:
                result_ = Value::make_bool(value_compare_less_equal(right, left, expr.diagnostic_token));
                return;
            case BinaryOp::Range:
                if (left.getType() != right.getType()) {
                    fail(expr.diagnostic_token, "Range bounds must be of the same type");
                }
                result_ = Value::make_range(left, right, false);
                return;
            case BinaryOp::RangeInclusiveEnd:
                if (left.getType() != right.getType()) {
                    fail(expr.diagnostic_token, "Range bounds must be of the same type");
                }
                result_ = Value::make_range(left, right, true);
                return;
            case BinaryOp::In:
                result_ = belongs_to(right, left, expr.diagnostic_token);
                return;
            case BinaryOp::NotIn:
                result_ = Value::make_bool(!as_bool(belongs_to(right, left, expr.diagnostic_token), expr.diagnostic_token));
                return;
            case BinaryOp::Union:
                result_ = set_union(left, right, expr.diagnostic_token);
                return;
            case BinaryOp::Intersection:
                result_ = set_intersection(left, right, expr.diagnostic_token);
                return;

            default:
                fail(expr.diagnostic_token, "Unsupported binary operator");
        }
    }

    void visit(const UnaryExpr& expr) override {
        switch (expr.op) {
            case UnaryOp::Not: {
                Value right = evaluate(*expr.right);
                result_ = Value::make_bool(!as_bool(right, expr.diagnostic_token));
                return;
            }
            case UnaryOp::Negate: {
                if (const auto* literal = dynamic_cast<const NumberExpr*>(expr.right.get())) {
                    result_ = Value::make_int(parse_negated_integer_literal(literal->value, literal->diagnostic_token));
                    return;
                }

                Value right = evaluate(*expr.right);
                result_ = value_negate(right, expr.diagnostic_token);
                return;
            }
            case UnaryOp::AddressOf:
            case UnaryOp::MutableAddressOf: {
                bool is_mut = (expr.op == UnaryOp::MutableAddressOf);
                std::shared_ptr<Binding> binding;
                if (const auto* ident = dynamic_cast<const IdentifierExpr*>(expr.right.get())) {
                    binding = lookup_binding(ident->name, ident->diagnostic_token);
                } else if (const auto* deref = dynamic_cast<const UnaryExpr*>(expr.right.get())) {
                    if (deref->op == UnaryOp::Deref) {
                        Value ref_val = evaluate_deref_target(*deref->right);
                        if (!ref_val.isBinding()) {
                            fail(expr.diagnostic_token, "Cannot take address of non-lvalue dereference");
                        }
                        binding = ref_val.asBinding();
                        if (is_mut) {
                            const auto* ref_type = dynamic_cast<const ReferenceType*>(ref_val.getType().get());
                            if (!ref_type || !ref_type->is_mut()) {
                                fail(expr.diagnostic_token, "Cannot take mutable address through immutable reference");
                            }
                        }
                    } else {
                        fail(expr.diagnostic_token, "Cannot take address of non-lvalue expression");
                    }
                } else {
                    fail(expr.diagnostic_token, "Cannot take address of non-lvalue expression");
                }

                auto ref_type = std::make_shared<ReferenceType>(binding->getFC(), is_mut);
                result_ = Value(std::move(ref_type), Value::BindingTag{std::move(binding)});
                return;
            }
            case UnaryOp::PointerType:
            case UnaryOp::MutablePointerType: {
                bool is_mut = (expr.op == UnaryOp::MutablePointerType);
                Value target_type = evaluate(*expr.right);
                result_ = make_reference_set(std::move(target_type), is_mut, expr.diagnostic_token);
                return;
            }
            case UnaryOp::Deref: {
                Value right = evaluate_deref_target(*expr.right);
                if (right.isHeapAllocation()) {
                    result_ = dereference_heap_allocation(right, expr.diagnostic_token);
                } else if (right.isBinding()) {
                    auto binding = right.asBinding();
                    Value cv = binding->getCV();
                    if (binding->ownsCV() && is_uncopyable(cv.getType())) {
                        fail(expr.diagnostic_token, "Cannot copy unique or droppable value through reference");
                    }
                    result_ = cv;
                } else {
                    if (const Value* deref_trait = get_registered_item("traits.dereferenceable")) {
                        if (const Value* impl = registered_impl_for(*deref_trait, right.getType())) {
                            if (impl->isStructInstance()) {
                                const auto& fields = *impl->asStructInstance().fields;
                                auto deref_it = fields.find("deref");
                                if (deref_it != fields.end() && deref_it->second.isLambda()) {
                                    result_ = call_callable_with_values(deref_it->second, {right}, expr.diagnostic_token, {false}, {false});
                                    return;
                                }
                            }
                        }
                    }
                    fail(expr.diagnostic_token, "Cannot dereference non-pointer value");
                }
                return;
            }
            case UnaryOp::Complement: {
                Value right = evaluate(*expr.right);
                result_ = set_complement(right, expr.diagnostic_token);
                return;
            }
            default:
                fail(expr.diagnostic_token, "Unsupported unary operator");
        }
    }

    void visit(const GroupingExpr& expr) override {
        result_ = evaluate(*expr.expression);
    }

    void visit(const NumberExpr& expr) override {
        result_ = Value::make_int(parse_positive_integer_literal(expr.value, expr.diagnostic_token));
    }

    void visit(const StringExpr& expr) override {
        frontend::token_type t = expr.diagnostic_token.type;
        if (t == frontend::token_type::fstring_head || t == frontend::token_type::fstring_middle || t == frontend::token_type::fstring_tail || t == frontend::token_type::fstring_literal) {
            result_ = Value::make_string(decode_fstring_part(expr.value, t, expr.diagnostic_token));
            return;
        }
        result_ = Value::make_string(decode_quoted_literal(expr.value, expr.diagnostic_token));
    }

    void visit(const FStringExpr& expr) override {
        std::string res;
        for (const auto& part : expr.parts) {
            result_ = evaluate(*part);
            if (result_.isString()) {
                res += result_.asString();
            } else if (result_.isChar()) {
                append_utf8(res, result_.asChar());
            } else {
                res += result_.toString();
            }
        }
        result_ = Value::make_string(res);
    }

    void visit(const CharExpr& expr) override {
        std::string decoded = decode_quoted_literal(expr.value, expr.diagnostic_token);
        uint32_t codepoint = decode_utf8_char(decoded, expr.diagnostic_token);
        result_ = Value::make_char(codepoint);
    }



    void visit(const IdentifierExpr& expr) override {
        result_ = builtin_identifier(expr.name, expr.diagnostic_token);
    }

    void visit(const IntrinsicExpr& expr) override {
        result_ = builtin_intrinsic(expr.name, expr.diagnostic_token);
    }



    void visit(const SymbolicConstantExpr& expr) override {
        result_ = Value::make_symbol(std::string(expr.value));
    }

    void visit(const EnumeratedSetExpr& expr) override {
        std::vector<Value> elements;
        elements.reserve(expr.elements.size());
        for (const auto& element : expr.elements) {
            elements.push_back(evaluate(*element));
        }
        result_ = Value::make_enumerated_set(std::move(elements));
    }

    void visit(const ConstructedSetExpr& expr) override {
        result_ = Value::make_constructed_set(expr, capture_scopes());
    }

    void visit(const AnonymousStructLiteralExpr& expr) override {
        fail(expr.diagnostic_token, "Anonymous struct literal requires a concrete struct context");
    }

    void visit(const IfExpr& expr) override {
        if (as_bool(evaluate(*expr.condition), expr.diagnostic_token)) {
            result_ = evaluate(*expr.then_branch);
        } else {
            result_ = evaluate(*expr.else_branch);
        }
    }

    void visit(const WhileExpr& expr) override {
        int64_t iterations = 0;
        while (as_bool(evaluate(*expr.condition), expr.diagnostic_token)) {
            if (iterations++ >= MAX_LOOP_ITERATIONS) {
                fail(expr.diagnostic_token, "Loop iteration limit exceeded");
            }
            run_loop_body(*expr.body);
        }
        result_ = VoidVal();
    }

    void visit(const ForExpr& expr) override {
        Value iterable = evaluate(*expr.iterable);
        if (!iterable.isRange()) {
            fail(expr.diagnostic_token, "for loops only support Range iteration for now");
        }

        auto range = iterable.asRange();
        Value current = *range.start;
        int64_t iterations = 0;
        auto in_range = [&]() {
            return range.inclusive_end
                ? value_compare_less_equal(current, *range.end, expr.diagnostic_token)
                : value_compare_less(current, *range.end, expr.diagnostic_token);
        };

        while (in_range()) {
            if (iterations++ >= MAX_LOOP_ITERATIONS) {
                fail(expr.diagnostic_token, "Loop iteration limit exceeded");
            }

            scopes_.push_back(std::make_shared<Scope>());
            bool loop_scope_active = true;
            try {
                bind_loop_iterator(expr.iterator_binding, current, expr.diagnostic_token);
                run_loop_body(*expr.body);
                leave_scope(nullptr, expr.diagnostic_token);
                loop_scope_active = false;
            } catch (...) {
                if (loop_scope_active) {
                    scopes_.pop_back();
                }
                throw;
            }

            if (current.isInt()) {
                current = Value::make_int(current.asInt() + BigInt(1));
            } else if (current.isChar()) {
                current = Value::make_char(current.asChar() + 1);
            } else {
                fail(expr.diagnostic_token, "Type not iterable in range");
            }
        }

        result_ = VoidVal();
    }

    void visit(const LambdaExpr& expr) override {
        result_ = Value::make_lambda(expr, capture_scopes());
    }

    void visit(const SignatureExpr& expr) override {
        result_ = Value::make_signature(expr, capture_scopes());
    }

    void visit(const BlockExpr& expr) override {
        scopes_.push_back(std::make_shared<Scope>());
        bool block_scope_active = true;
        try {
            for (const auto& stmt : expr.statements) {
                execute_stmt(*stmt);
            }
            leave_scope(nullptr, expr.diagnostic_token);
            block_scope_active = false;
            result_ = VoidVal();
        } catch (const BreakSignal& signal) {
            leave_scope(signal.handoff_binding, expr.diagnostic_token);
            block_scope_active = false;
            result_ = signal.value;
        } catch (...) {
            if (block_scope_active) {
                scopes_.pop_back();
            }
            throw;
        }
    }

    void visit(const StructExpr& expr) override {
        auto type = std::make_shared<StructType>(&expr);
        result_ = Value::make_type(std::move(type));
    }

    void visit(const EnumExpr& expr) override {
        result_ = Value::make_enum_family(expr.node_id, expr.variants);
    }

    void visit(const CallExpr& expr) override {
        if (const auto* intrinsic = dynamic_cast<const IntrinsicExpr*>(expr.callee.get())) {
            if (is_name(intrinsic->name, "`import")) {
                result_ = call_import(expr.args, expr.diagnostic_token);
                return;
            }
        }

        Value callee = evaluate(*expr.callee);
        if (callee.isLambda()) {
            result_ = call_lambda(callee.asLambdaTag(), expr.args, expr.diagnostic_token);
            return;
        }
        if (callee.isHostFunction()) {
            result_ = call_host_function(callee.asHostFunction(), expr.args, expr.diagnostic_token);
            return;
        }

        if (const Value* callable_trait = callable_trait_value()) {
            if (const Value* impl = registered_impl_for(*callable_trait, callee.getType())) {
                result_ = call_registered_callable(callee, *impl, expr.args, expr.diagnostic_token);
                return;
            }
        }

        if (callee.isType() && is_struct_type(callee.asType())) {
            result_ = call_struct_constructor(callee.asType(), expr.args, expr.diagnostic_token);
            return;
        }

        if (callee.isType()) {
            if (const Value* callable_trait = callable_trait_value()) {
                if (const Value* impl = registered_impl_for(*callable_trait, callee.getType())) {
                    result_ = call_registered_callable(callee, *impl, expr.args, expr.diagnostic_token);
                    return;
                }
            }
        }

        fail(expr.diagnostic_token, "Value is not callable: " + callee.toString());
    }

    void visit(const IndexExpr& expr) override {
        Value target = evaluate(*expr.target);
        if (const Value* indexable_trait = indexable_trait_value()) {
            if (const Value* impl = registered_impl_for(*indexable_trait, target.getType())) {
                if (!impl->isStructInstance()) {
                    fail(expr.diagnostic_token, "`Indexable implementation must be a struct instance");
                }
                const auto& fields = *impl->asStructInstance().fields;
                auto read_it = fields.find("read");
                if (read_it == fields.end()) {
                    fail(expr.diagnostic_token, "`Indexable implementation is missing method 'read'");
                }
                Value lowered_args = lower_struct_arguments(target, *impl, "index_space", expr.args, expr.diagnostic_token);
                result_ = call_callable_with_values(read_it->second, {target, lowered_args}, expr.diagnostic_token, {false, true}, {false, true});
                return;
            }
        }

        if (!target.isList() && !target.isString()) {
            fail(expr.diagnostic_token, "Only list and string indexing/slicing are supported");
        }
        if (expr.args.size() != 1 || expr.args[0].name.has_value()) {
            fail(expr.diagnostic_token, "List indexing requires exactly one positional argument");
        }
        Value index_val = evaluate(*expr.args[0].value);
        if (index_val.isInt()) {
            if (!target.isList()) {
                fail(expr.diagnostic_token, "Only list indexing is supported for integer indices");
            }
            BigInt index_big = index_val.asInt();
            if (!index_big.fits_int64()) {
                fail(expr.diagnostic_token, "List index is out of range for standard indexing");
            }
            int64_t index = index_big.to_int64();
            const auto& list_elems = target.asList();
            if (index < 0 || index >= static_cast<int64_t>(list_elems.size())) {
                fail(expr.diagnostic_token, "List index out of bounds: " + std::to_string(index));
            }
            result_ = list_elems[index];
            return;
        } else if (index_val.isRange()) {
            if (!target.isList() && !target.isString()) {
                fail(expr.diagnostic_token, "Only list and string slicing are supported");
            }
            const auto& range = index_val.asRange();
            if (!range.start->isInt() || !range.end->isInt()) {
                fail(expr.diagnostic_token, "Slice range must have integer bounds");
            }
            int64_t start = range.start->asInt().to_int64();
            int64_t end = range.end->asInt().to_int64();
            if (range.inclusive_end) {
                end++;
            }
            int64_t size = target.isList() ? target.asList().size() : target.asString().size();
            if (start < 0 || end < start || end > size) {
                fail(expr.diagnostic_token, "Slice range out of bounds");
            }
            if (target.isList()) {
                const auto& list_elems = target.asList();
                std::vector<Value> sliced_elems(list_elems.begin() + start, list_elems.begin() + end);
                result_ = Value::make_list(std::move(sliced_elems));
            } else {
                result_ = Value::make_string(target.asString().substr(start, end - start));
            }
            return;
        }
        fail(expr.diagnostic_token, "List index must be an integer or a range");
    }


    void visit(const ListExpr& expr) override {
        std::vector<Value> elements;
        elements.reserve(expr.elements.size());
        for (const auto& element : expr.elements) {
            elements.push_back(evaluate(*element));
        }
        result_ = Value::make_list(std::move(elements));
    }


    void visit(const MatchExpr& expr) override {
        Value subject = evaluate(*expr.subject);

        for (const auto& arm : expr.arms) {
            Value pattern = evaluate(*arm.pattern);

            bool matched = false;
            if (has_setness(pattern)) {
                // Pattern is a set: test subject ∈ pattern
                Value result = belongs_to(pattern, subject, expr.diagnostic_token);
                matched = as_bool(result, expr.diagnostic_token);
            } else {
                // Pattern is a literal value (int, string, etc.): test equality
                matched = (subject == pattern);
            }

            if (matched) {
                result_ = evaluate(*arm.body);
                return;
            }
        }

        fail(expr.diagnostic_token, "Non-exhaustive match: no arm matched value " + display_string(subject));
    }

    void visit(const ExprStmt& stmt) override {
        Value value = evaluate(*stmt.expression);
        drop_value(value, stmt.diagnostic_token);
    }

    void visit(const LetStmt& stmt) override {
        Value constraint = stmt.binding.type_bound ? evaluate(*stmt.binding.type_bound) : VoidVal();
        Value initializer = evaluate_with_expected_type(*stmt.binding.initializer, constraint, stmt.diagnostic_token);
        enforce_constraint(constraint, initializer, stmt.diagnostic_token);
        retain_owned_value(initializer, stmt.diagnostic_token);

        auto binding = std::make_shared<Binding>(constraint, constraint, initializer, stmt.binding.is_final);
        define_binding(stmt.binding.name.lexeme, binding, stmt.binding.name);
        if (stmt.is_public && is_boot_top_level()) {
            publish_global_binding(stmt.binding.name.lexeme, binding, stmt.binding.name);
        }
        if (stmt.is_public && is_module_top_level()) {
            auto& exports = *module_export_stack_.back();
            std::string export_name = to_key(stmt.binding.name.lexeme);
            auto [_, inserted] = exports.emplace(export_name, binding);
            if (!inserted) {
                fail(stmt.binding.name, "Module export '" + export_name + "' is already defined");
            }
        }
    }

    void visit(const BreakStmt& stmt) override {
        TerminalValue terminal_value = stmt.value
            ? evaluate_terminal_expression(*stmt.value)
            : TerminalValue{VoidVal(), nullptr};

        if (terminal_value.handoff_binding) {
            disown_shared_heap_allocation_without_destroy(terminal_value.handoff_binding->getCV(), stmt.diagnostic_token);
            terminal_value.handoff_binding->setOwnsCV(false);
        }

        throw BreakSignal{std::move(terminal_value.value), std::move(terminal_value.handoff_binding)};
    }

    void visit(const AssignStmt& stmt) override {
        std::shared_ptr<Binding> binding;
        bool is_heap_assignment = false;
        bool is_index_assignment = false;
        std::shared_ptr<Value::HeapAllocationState> heap_state;
        const Value* trait_deref_assign_lambda = nullptr;
        Value trait_ref_val;
        Value trait_index_write;
        Value trait_index_target;
        Value trait_index_at;
        Value trait_index_constraint = VoidVal();

        if (const auto* target = dynamic_cast<const IdentifierExpr*>(stmt.target.get())) {
            binding = lookup_binding(target->name, target->diagnostic_token);
        } else if (const auto* index = dynamic_cast<const IndexExpr*>(stmt.target.get())) {
            Value target_val = evaluate(*index->target);
            const Value* index_assignable_trait = index_assignable_trait_value();
            if (index_assignable_trait == nullptr) {
                fail(stmt.diagnostic_token, "Indexed assignment is not supported");
            }
            const Value* impl = registered_impl_for(*index_assignable_trait, target_val.getType());
            if (impl == nullptr || !impl->isStructInstance()) {
                fail(stmt.diagnostic_token, "Indexed assignment is not supported for this value");
            }
            const auto& fields = *impl->asStructInstance().fields;
            auto write_it = fields.find("write");
            auto value_space_it = fields.find("value_space");
            if (write_it == fields.end()) {
                fail(stmt.diagnostic_token, "`IndexAssignable implementation is missing method 'write'");
            }
            if (value_space_it == fields.end()) {
                fail(stmt.diagnostic_token, "`IndexAssignable implementation is missing method 'value_space'");
            }

            trait_index_target = target_val;
            trait_index_at = lower_struct_arguments(target_val, *impl, "index_space", index->args, stmt.diagnostic_token);
            trait_index_constraint = call_callable_with_values(value_space_it->second, {trait_index_target, trait_index_at}, stmt.diagnostic_token, {false, true}, {false, true});
            trait_index_write = write_it->second;
            is_index_assignment = true;
        } else if (const auto* deref = dynamic_cast<const UnaryExpr*>(stmt.target.get())) {
            if (deref->op == UnaryOp::Deref) {
                Value ref_val = evaluate_deref_target(*deref->right);
                if (ref_val.isHeapAllocation()) {
                    heap_state = ref_val.asHeapAllocation().state;
                    if (!heap_state || heap_state->destroyed) {
                        fail(stmt.diagnostic_token, "Cannot assign to dereference of destroyed heap allocation");
                    }
                    is_heap_assignment = true;
                } else if (ref_val.isBinding()) {
                    const auto* ref_type = dynamic_cast<const ReferenceType*>(ref_val.getType().get());
                    if (!ref_type) {
                        fail(stmt.diagnostic_token, "Invalid reference type");
                    }
                    if (!ref_type->is_mut()) {
                        fail(stmt.diagnostic_token, "Cannot assign to immutable reference");
                    }
                    binding = ref_val.asBinding();
                } else {
                    if (const Value* deref_mut_trait = get_registered_item("traits.dereferenceable_mut")) {
                        if (const Value* impl = registered_impl_for(*deref_mut_trait, ref_val.getType())) {
                            if (impl->isStructInstance()) {
                                const auto& fields = *impl->asStructInstance().fields;
                                auto deref_assign_it = fields.find("deref_assign");
                                if (deref_assign_it != fields.end() && deref_assign_it->second.isLambda()) {
                                    trait_deref_assign_lambda = &deref_assign_it->second;
                                    trait_ref_val = ref_val;
                                }
                            }
                        }
                    }
                    if (!trait_deref_assign_lambda) {
                        fail(stmt.diagnostic_token, "Cannot assign to dereference of non-pointer value");
                    }
                }
            } else {
                fail(stmt.diagnostic_token, "Unsupported assignment target");
            }
        } else {
            fail(stmt.diagnostic_token, "Only identifier, index, and dereference assignment are supported");
        }

        Value constraint = is_heap_assignment ? VoidVal() :
            (trait_deref_assign_lambda ? VoidVal() :
            (is_index_assignment ? trait_index_constraint : binding->getFC()));
        Value value = evaluate_with_expected_type(*stmt.value, constraint, stmt.diagnostic_token);

        try {
            Value current_val;
            if (stmt.op.type != token_type::equal) {
                if (is_heap_assignment) {
                    current_val = *heap_state->stored;
                } else if (is_index_assignment) {
                    current_val = evaluate(*stmt.target);
                } else if (trait_deref_assign_lambda) {
                    if (const Value* deref_trait = get_registered_item("traits.dereferenceable")) {
                        if (const Value* read_impl = registered_impl_for(*deref_trait, trait_ref_val.getType())) {
                            if (read_impl->isStructInstance()) {
                                const auto& read_fields = *read_impl->asStructInstance().fields;
                                auto deref_it = read_fields.find("deref");
                                if (deref_it != read_fields.end() && deref_it->second.isLambda()) {
                                    current_val = call_callable_with_values(deref_it->second, {trait_ref_val}, stmt.diagnostic_token, {false}, {false});
                                } else fail(stmt.diagnostic_token, "Cannot read trait value for compound assignment");
                            } else fail(stmt.diagnostic_token, "Cannot read trait value for compound assignment");
                        } else fail(stmt.diagnostic_token, "Cannot read trait value for compound assignment");
                    } else fail(stmt.diagnostic_token, "Cannot read trait value for compound assignment");
                } else {
                    current_val = binding->getCV();
                }
            }
            switch (stmt.op.type) {
                case token_type::equal:
                    break;
                case token_type::plus_equal:
                    value = value_arithmetic(current_val, value, BinaryOp::Add, stmt.op);
                    break;
                case token_type::minus_equal:
                    value = value_arithmetic(current_val, value, BinaryOp::Sub, stmt.op);
                    break;
                case token_type::star_equal:
                    value = value_arithmetic(current_val, value, BinaryOp::Mul, stmt.op);
                    break;
                case token_type::slash_equal:
                    value = value_arithmetic(current_val, value, BinaryOp::Div, stmt.op);
                    break;
                case token_type::percent_equal:
                    value = value_arithmetic(current_val, value, BinaryOp::Mod, stmt.op);
                    break;
                default:
                    fail(stmt.op, "Unsupported assignment operator");
            }
        } catch (const std::out_of_range& e) {
            fail(stmt.op, e.what());
        }

        if (trait_deref_assign_lambda) {
            call_callable_with_values(*trait_deref_assign_lambda, {trait_ref_val, value}, stmt.diagnostic_token, {false, false}, {false, true});
            return;
        }

        if (is_index_assignment) {
            enforce_constraint(trait_index_constraint, value, stmt.diagnostic_token);
            call_callable_with_values(trait_index_write, {trait_index_target, trait_index_at, value}, stmt.diagnostic_token, {false, true, true}, {false, true, true});
            return;
        }

        if (is_heap_assignment) {
            Value old_value = heap_state->stored ? *heap_state->stored : VoidVal();
            retain_owned_value(value, stmt.diagnostic_token);
            drop_value(old_value, stmt.diagnostic_token);
            heap_state->stored = std::make_shared<Value>(std::move(value));
        } else {
            enforce_constraint(binding->getFC(), value, stmt.diagnostic_token);
            retain_owned_value(value, stmt.diagnostic_token);
            if (binding->ownsCV()) {
                Value old_value = binding->getCV();
                if (has_drop(old_value.getType())) {
                    binding->setOwnsCV(false);
                    drop_value(old_value, stmt.diagnostic_token);
                }
            }
            binding->setCV(std::move(value));
            binding->setOwnsCV(true);
        }
    }

    void visit(const IfStmt& stmt) override {
        if (as_bool(evaluate(*stmt.condition), stmt.diagnostic_token)) {
            execute_stmt(*stmt.then_branch);
        } else if (stmt.else_branch) {
            execute_stmt(*stmt.else_branch);
        }
    }

    void visit(const DebugStmt& stmt) override {
        for (const auto& s : stmt.statements) {
            execute_stmt(*s);
        }
    }
};

} // namespace

class Session::Impl {
    struct SourceUnit {
        std::string label;
        std::string source;
        std::vector<std::unique_ptr<frontend::Stmt>> stmts;
    };

public:
    explicit Impl(std::ostream& out, bool testing_enabled = false) : evaluator(out, testing_enabled) {}
    ~Impl() {
        evaluator.shutdown();
        sources.clear();
    }

    SessionExpectations getExpectations() const {
        return evaluator.expectations;
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
        evaluator.execute(stmts);
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) {
        evaluator.execute(stmts, std::move(label));
    }

    void execute_source(std::string source, std::string label, bool boot) {
        auto unit = std::make_unique<SourceUnit>();
        unit->source = std::move(source);
        unit->label = std::move(label);
        std::string error_label = unit->label;

        try {
            auto tokens = frontend::tokenize(unit->source);
            unit->stmts = frontend::parse(tokens);

            SourceUnit* loaded = unit.get();
            sources.push_back(std::move(unit));

            if (boot) {
                evaluator.execute_boot(loaded->stmts, loaded->label);
            } else {
                evaluator.execute(loaded->stmts, loaded->label);
            }
        } catch (const ScriptExit&) {
            throw;
        } catch (const std::exception& e) {
            if (error_label.empty()) {
                throw;
            }
            throw std::runtime_error(error_label + ": " + e.what());
        }
    }

    void set_chirp_root(std::string path) {
        evaluator.set_chirp_root(std::move(path));
    }

private:
    Evaluator evaluator;
    std::vector<std::unique_ptr<SourceUnit>> sources;
};

Session::Session(std::ostream& out, bool testing_enabled) : impl_(std::make_unique<Impl>(out, testing_enabled)) {}
Session::~Session() = default;
Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&&) noexcept = default;

void Session::execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
    impl_->execute(stmts);
}

void Session::execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) {
    impl_->execute(stmts, std::move(label));
}

void Session::execute_source(std::string source, std::string label) {
    impl_->execute_source(std::move(source), std::move(label), false);
}

void Session::execute_boot_source(std::string source, std::string label) {
    impl_->execute_source(std::move(source), std::move(label), true);
}

void Session::set_chirp_root(std::string path) {
    impl_->set_chirp_root(std::move(path));
}

SessionExpectations Session::getExpectations() const {
    return impl_->getExpectations();
}

void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::ostream& out) {
    Session session(out);
    session.execute(stmts);
}

} // namespace chirp::interpreter
