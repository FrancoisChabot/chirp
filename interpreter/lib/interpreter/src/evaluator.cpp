#include "chirp/interpreter.h"

#include "chirp/frontend.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace chirp::interpreter {
namespace {

using namespace chirp::frontend;

struct BreakSignal {
    Value value;
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

std::string display_string(const Value& value) {
    if (value.isString()) {
        return value.asString();
    }
    return value.toString();
}

bool is_name(std::string_view actual, std::string_view expected) {
    return actual == expected;
}

bool is_harness_intrinsic(std::string_view name) {
    return is_name(name, "`expect_stdout") ||
        is_name(name, "`expect_exit") ||
        is_name(name, "`expect_test_failure");
}

constexpr int64_t MAX_LOOP_ITERATIONS = 1'000'000;

class Evaluator : public ASTVisitor, public StmtVisitor {
public:
    SessionExpectations expectations;

    explicit Evaluator(std::ostream& out) : out_(out) {
        scopes_.emplace_back();
    }

    void execute(const std::vector<std::unique_ptr<Stmt>>& stmts) {
        for (const auto& stmt : stmts) {
            execute_stmt(*stmt);
        }
    }

    void execute_boot(const std::vector<std::unique_ptr<Stmt>>& stmts) {
        bool was_boot_mode = boot_mode_;
        boot_mode_ = true;
        try {
            execute(stmts);
            boot_mode_ = was_boot_mode;
        } catch (...) {
            boot_mode_ = was_boot_mode;
            throw;
        }
    }

private:
    using Scope = std::unordered_map<std::string, std::shared_ptr<Binding>>;

    std::ostream& out_;
    std::vector<Scope> scopes_;
    Value result_;
    bool boot_mode_ = false;

    Value evaluate(const Expr& expr) {
        expr.accept(*this);
        return result_;
    }

    void execute_stmt(const Stmt& stmt) {
        stmt.accept(*this);
    }

    std::shared_ptr<Binding> lookup_binding(std::string_view name, const token& diag) const {
        std::string key = to_key(name);
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            auto found = scope->find(key);
            if (found != scope->end()) {
                return found->second;
            }
        }
        fail(diag, "Undefined identifier '" + key + "'");
    }

    void define_binding(std::string_view name, std::shared_ptr<Binding> binding, const token& diag) {
        if (diag.type == token_type::intrinsic && !(boot_mode_ && scopes_.size() == 1)) {
            fail(diag, "Backtick-prefixed bindings may only be defined by top-level boot files");
        }

        std::string key = to_key(name);
        if (!scopes_.empty()) {
            for (auto scope = scopes_.begin(); scope != std::prev(scopes_.end()); ++scope) {
                auto found = scope->find(key);
                if (found != scope->end() && found->second->isFinal()) {
                    fail(diag, "Identifier '" + key + "' cannot shadow final binding");
                }
            }
        }

        auto [_, inserted] = scopes_.back().emplace(key, std::move(binding));
        if (!inserted) {
            fail(diag, "Identifier '" + key + "' is already defined in this scope");
        }
    }

    static bool as_bool(const Value& value, const token& diag) {
        if (!value.isBool()) {
            fail(diag, "Expected Bool, got " + value.toString());
        }
        return value.asBool();
    }

    static int64_t as_int(const Value& value, const token& diag) {
        if (!value.isInt()) {
            fail(diag, "Expected int, got " + value.toString());
        }
        return value.asInt();
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
                if (!left_res.asBool()) return Value::make_bool(false);
                return belongs_to(*comp.right, value, diag);
            }
        }

        if (!set.isConstructedSet()) {
            return belongsTo(set, value);
        }

        const ConstructedSetExpr& expr = set.asConstructedSet();
        Value bound = expr.binding.type_bound ? evaluate(*expr.binding.type_bound) : Any();
        Value in_bound = belongs_to(bound, value, expr.diagnostic_token);
        if (!in_bound.isBool()) {
            fail(expr.diagnostic_token, "Set bound belonging predicate did not return Bool");
        }
        if (!in_bound.asBool()) {
            return Value::make_bool(false);
        }

        scopes_.emplace_back();
        try {
            auto binding = std::make_shared<Binding>(bound, bound, value, expr.binding.is_final);
            define_binding(expr.binding.name.lexeme, std::move(binding), expr.binding.name);

            Value predicate_result = evaluate(*expr.condition);
            if (!predicate_result.isBool()) {
                fail(expr.diagnostic_token, "Constructed set predicate must evaluate to Bool");
            }

            scopes_.pop_back();
            return predicate_result;
        } catch (...) {
            scopes_.pop_back();
            throw;
        }
    }

    static void require_set_operand(const Value& value, const token& diag) {
        if (!value.getType()->hasSetness()) {
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
            int64_t current = range.start;
            int64_t iterations = 0;
            auto in_range = [&]() {
                return range.inclusive_end ? current <= range.end : current < range.end;
            };

            while (in_range()) {
                if (iterations++ >= MAX_LOOP_ITERATIONS) {
                    fail(diag, "Finite set materialization limit exceeded");
                }
                append_unique(elements, Value::make_int(current));

                if (current == std::numeric_limits<int64_t>::max()) {
                    break;
                }
                current += 1;
            }
            return elements;
        }

        if (set.isConstructedSet()) {
            const ConstructedSetExpr& expr = set.asConstructedSet();
            if (!expr.binding.type_bound) {
                fail(diag, "Cannot materialize unbounded constructed set");
            }

            Value bound = evaluate(*expr.binding.type_bound);
            std::vector<Value> candidates = finite_elements(bound, expr.diagnostic_token);
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
            std::vector<Value> elements = finite_elements(*comp.left, diag);
            if (comp.op == Value::CompositeSetOp::Union) {
                for (const auto& element : finite_elements(*comp.right, diag)) {
                    append_unique(elements, element);
                }
            } else { // Intersection
                std::vector<Value> intersected;
                for (const auto& element : elements) {
                    if (as_bool(belongs_to(*comp.right, element, diag), diag)) {
                        append_unique(intersected, element);
                    }
                }
                return intersected;
            }
            return elements;
        }

        if (set == Empty()) {
            return {};
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

    bool is_subset(const Value& left, const Value& right, const token& diag) {
        require_set_operand(left, diag);
        require_set_operand(right, diag);

        for (const auto& element : finite_elements(left, diag)) {
            if (!as_bool(belongs_to(right, element, diag), diag)) {
                return false;
            }
        }
        return true;
    }

    void enforce_constraint(const Value& constraint, const Value& value, const token& diag) {
        Value belongs = belongs_to(constraint, value, diag);
        if (!belongs.isBool()) {
            fail(diag, "Constraint belonging predicate did not return Bool");
        }
        if (!belongs.asBool()) {
            fail(diag, "Constraint violation: " + display_string(value) + " does not belong to " + constraint.toString());
        }
    }

    Value builtin_identifier(std::string_view name, const token& diag) const {
        if (is_name(name, "int")) return Value::make_type(getIntType());
        if (is_name(name, "string")) return Value::make_type(getStringType());
        if (is_name(name, "Type")) return TypeVal();
        if (is_name(name, "any")) return Any();
        if (is_name(name, "empty")) return Empty();
        if (is_name(name, "set")) return Set();
        if (is_name(name, "Void")) return Void();
        return lookup_binding(name, diag)->getCV();
    }

    Value builtin_intrinsic(std::string_view name, const token& diag) const {
        if (is_harness_intrinsic(name)) return VoidVal();
        if (is_name(name, "`__boot_bind")) {
            fail(diag, "`__boot_bind is only available as a boot-time call");
        }
        return lookup_binding(name, diag)->getCV();
    }

    Value boot_bind(std::string_view name, const token& diag) const {
        if (is_name(name, "print_func")) return Value::make_host_function(Value::HostFunction::Print);
        if (is_name(name, "typeof_func")) return Value::make_host_function(Value::HostFunction::TypeOf);
        if (is_name(name, "exit_func")) return Value::make_host_function(Value::HostFunction::Exit);
        if (is_name(name, "true_val")) return True();
        if (is_name(name, "false_val")) return False();
        if (is_name(name, "bool_type")) return Bool();
        if (is_name(name, "undecided_val")) return UndecidedVal();
        if (is_name(name, "void_val")) return VoidVal();
        if (is_name(name, "any_val")) return Any();
        if (is_name(name, "empty_val")) return Empty();
        if (is_name(name, "set_val")) return Set();
        if (is_name(name, "type_val_and_type")) return TypeVal();
        fail(diag, "Unknown boot binding '" + to_key(name) + "'");
    }

    Value call_boot_bind(const std::vector<Argument>& args, const token& diag) {
        if (!boot_mode_) {
            fail(diag, "`__boot_bind is only available while evaluating boot files");
        }
        if (args.size() != 1 || args.front().name.has_value()) {
            fail(diag, "`__boot_bind expects one positional string argument");
        }
        Value name = evaluate(*args.front().value);
        if (!name.isString()) {
            fail(diag, "`__boot_bind expects a string argument");
        }
        return boot_bind(name.asString(), diag);
    }

    Value call_host_function(Value::HostFunction fn, const std::vector<Argument>& args, const token& diag) {
        switch (fn) {
            case Value::HostFunction::Print: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`print expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                out_ << display_string(value) << '\n';
                return VoidVal();
            }
            case Value::HostFunction::TypeOf: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`typeof expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                return Value::make_type(value.getType());
            }
            case Value::HostFunction::Exit: {
                if (args.size() != 1 || args.front().name.has_value()) {
                    fail(diag, "`exit expects one positional argument");
                }
                Value value = evaluate(*args.front().value);
                int64_t code = as_int(value, diag);
                if (code < 0 || code > 255) {
                    fail(diag, "`exit expects an integer exit code between 0 and 255");
                }
                throw ScriptExit(static_cast<int>(code));
            }
        }
        fail(diag, "Unknown host function");
    }

    int64_t binary_int(const Value& left, const Value& right, BinaryOp op, const token& diag) {
        int64_t a = as_int(left, diag);
        int64_t b = as_int(right, diag);
        switch (op) {
            case BinaryOp::Add: return a + b;
            case BinaryOp::Sub: return a - b;
            case BinaryOp::Mul: return a * b;
            case BinaryOp::Div:
                if (b == 0) fail(diag, "Division by zero");
                return a / b;
            case BinaryOp::Mod:
                if (b == 0) fail(diag, "Modulo by zero");
                return a % b;
            default:
                fail(diag, "Unsupported integer operator");
        }
    }

    Value call_lambda(const LambdaExpr& lambda, const std::vector<Argument>& args, const token& diag) {
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

                arg_values[index] = evaluate(*arg.value);
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
                arg_values[i] = evaluate(*args[i].value);
            }
        }

        scopes_.emplace_back();
        try {
            for (size_t i = 0; i < lambda.parameters.size(); ++i) {
                const NamedBinding& param = lambda.parameters[i];
                Value constraint = param.type_bound ? evaluate(*param.type_bound) : Any();
                enforce_constraint(constraint, arg_values[i], param.name);

                auto binding = std::make_shared<Binding>(constraint, constraint, std::move(arg_values[i]), param.is_final);
                define_binding(param.name.lexeme, std::move(binding), param.name);
            }

            Value return_value = evaluate(*lambda.body);
            Value return_constraint = lambda.return_bound ? evaluate(*lambda.return_bound) : Any();
            enforce_constraint(return_constraint, return_value, lambda.diagnostic_token);

            scopes_.pop_back();
            return return_value;
        } catch (...) {
            scopes_.pop_back();
            throw;
        }
    }

    void bind_loop_iterator(const NamedBinding& binding, Value value, const token& diag) {
        Value constraint = binding.type_bound ? evaluate(*binding.type_bound) : Any();
        enforce_constraint(constraint, value, diag);

        auto iterator_binding = std::make_shared<Binding>(constraint, constraint, std::move(value), binding.is_final);
        define_binding(binding.name.lexeme, std::move(iterator_binding), binding.name);
    }

    void run_loop_body(const Expr& body) {
        evaluate(body);
    }

    void visit(const BinaryExpr& expr) override {
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
                result_ = Value::make_int(binary_int(left, right, expr.op, expr.diagnostic_token));
                return;
            case BinaryOp::Eq:
                result_ = Value::make_bool(left == right);
                return;
            case BinaryOp::Neq:
                result_ = Value::make_bool(left != right);
                return;
            case BinaryOp::Lt:
                result_ = Value::make_bool(as_int(left, expr.diagnostic_token) < as_int(right, expr.diagnostic_token));
                return;
            case BinaryOp::Lte:
                result_ = Value::make_bool(as_int(left, expr.diagnostic_token) <= as_int(right, expr.diagnostic_token));
                return;
            case BinaryOp::Gt:
                result_ = Value::make_bool(as_int(left, expr.diagnostic_token) > as_int(right, expr.diagnostic_token));
                return;
            case BinaryOp::Gte:
                result_ = Value::make_bool(as_int(left, expr.diagnostic_token) >= as_int(right, expr.diagnostic_token));
                return;
            case BinaryOp::Range:
                result_ = Value::make_range(
                    as_int(left, expr.diagnostic_token),
                    as_int(right, expr.diagnostic_token),
                    false);
                return;
            case BinaryOp::RangeInclusiveEnd:
                result_ = Value::make_range(
                    as_int(left, expr.diagnostic_token),
                    as_int(right, expr.diagnostic_token),
                    true);
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
            case BinaryOp::Subset:
                result_ = Value::make_bool(is_subset(left, right, expr.diagnostic_token));
                return;
            case BinaryOp::ProperSubset:
                result_ = Value::make_bool(
                    is_subset(left, right, expr.diagnostic_token) &&
                    !is_subset(right, left, expr.diagnostic_token));
                return;
            case BinaryOp::NotSubset:
                result_ = Value::make_bool(!is_subset(left, right, expr.diagnostic_token));
                return;
            case BinaryOp::Superset:
                result_ = Value::make_bool(is_subset(right, left, expr.diagnostic_token));
                return;
            case BinaryOp::ProperSuperset:
                result_ = Value::make_bool(
                    is_subset(right, left, expr.diagnostic_token) &&
                    !is_subset(left, right, expr.diagnostic_token));
                return;
            case BinaryOp::NotSuperset:
                result_ = Value::make_bool(!is_subset(right, left, expr.diagnostic_token));
                return;
            default:
                fail(expr.diagnostic_token, "Unsupported binary operator");
        }
    }

    void visit(const UnaryExpr& expr) override {
        Value right = evaluate(*expr.right);
        switch (expr.op) {
            case UnaryOp::Not:
                result_ = Value::make_bool(!as_bool(right, expr.diagnostic_token));
                return;
            case UnaryOp::Negate:
                result_ = Value::make_int(-as_int(right, expr.diagnostic_token));
                return;
            default:
                fail(expr.diagnostic_token, "Unsupported unary operator");
        }
    }

    void visit(const GroupingExpr& expr) override {
        result_ = evaluate(*expr.expression);
    }

    void visit(const NumberExpr& expr) override {
        std::string text(expr.value);
        if (text.find('.') != std::string::npos) {
            fail(expr.diagnostic_token, "Floating point literals are not supported yet");
        }

        size_t pos = 0;
        long long parsed = 0;
        try {
            parsed = std::stoll(text, &pos);
        } catch (const std::exception&) {
            fail(expr.diagnostic_token, "Invalid integer literal '" + text + "'");
        }

        if (pos != text.size() ||
            parsed < std::numeric_limits<int64_t>::min() ||
            parsed > std::numeric_limits<int64_t>::max()) {
            fail(expr.diagnostic_token, "Invalid integer literal '" + text + "'");
        }
        result_ = Value::make_int(static_cast<int64_t>(parsed));
    }

    void visit(const StringExpr& expr) override {
        result_ = Value::make_string(decode_quoted_literal(expr.value, expr.diagnostic_token));
    }

    void visit(const BoolExpr& expr) override {
        result_ = Value::make_bool(expr.value);
    }

    void visit(const IdentifierExpr& expr) override {
        result_ = builtin_identifier(expr.name, expr.diagnostic_token);
    }

    void visit(const IntrinsicExpr& expr) override {
        if (is_name(expr.name, "`expect_test_failure")) {
            expectations.has_expectations = true;
            expectations.expect_test_failure = true;
            result_ = VoidVal();
            return;
        }
        result_ = builtin_intrinsic(expr.name, expr.diagnostic_token);
    }

    void visit(const UndecidedExpr& expr) override {
        result_ = UndecidedVal();
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
        result_ = Value::make_constructed_set(expr);
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
        int64_t current = range.start;
        int64_t iterations = 0;
        auto in_range = [&]() {
            return range.inclusive_end ? current <= range.end : current < range.end;
        };

        while (in_range()) {
            if (iterations++ >= MAX_LOOP_ITERATIONS) {
                fail(expr.diagnostic_token, "Loop iteration limit exceeded");
            }

            scopes_.emplace_back();
            try {
                bind_loop_iterator(expr.iterator_binding, Value::make_int(current), expr.diagnostic_token);
                run_loop_body(*expr.body);
                scopes_.pop_back();
            } catch (...) {
                scopes_.pop_back();
                throw;
            }

            if (current == std::numeric_limits<int64_t>::max()) {
                break;
            }
            current += 1;
        }

        result_ = VoidVal();
    }

    void visit(const LambdaExpr& expr) override {
        result_ = Value::make_lambda(expr);
    }

    void visit(const BlockExpr& expr) override {
        scopes_.emplace_back();
        try {
            for (const auto& stmt : expr.statements) {
                execute_stmt(*stmt);
            }
            scopes_.pop_back();
            result_ = VoidVal();
        } catch (const BreakSignal& signal) {
            scopes_.pop_back();
            result_ = signal.value;
        } catch (...) {
            scopes_.pop_back();
            throw;
        }
    }

    void visit(const StructExpr& expr) override {
        fail(expr.diagnostic_token, "struct expressions are not supported yet");
    }

    void visit(const CallExpr& expr) override {
        if (const auto* intrinsic = dynamic_cast<const IntrinsicExpr*>(expr.callee.get())) {
            if (is_name(intrinsic->name, "`__boot_bind")) {
                result_ = call_boot_bind(expr.args, expr.diagnostic_token);
                return;
            }
            if (is_harness_intrinsic(intrinsic->name)) {
                if (is_name(intrinsic->name, "`expect_stdout")) {
                    if (expr.args.size() == 1) {
                        Value arg = evaluate(*expr.args.front().value);
                        if (arg.isString()) {
                            expectations.has_expectations = true;
                            if (!expectations.expected_stdout.has_value()) {
                                expectations.expected_stdout = arg.asString();
                            } else {
                                *expectations.expected_stdout += arg.asString();
                            }
                        }
                    }
                } else if (is_name(intrinsic->name, "`expect_exit")) {
                    if (expr.args.size() == 1) {
                        Value arg = evaluate(*expr.args.front().value);
                        if (arg.isInt()) {
                            expectations.has_expectations = true;
                            expectations.expected_exit = static_cast<int>(arg.asInt());
                        }
                    }
                }
                result_ = VoidVal();
                return;
            }
        }

        Value callee = evaluate(*expr.callee);
        if (callee.isLambda()) {
            result_ = call_lambda(callee.asLambda(), expr.args, expr.diagnostic_token);
            return;
        }
        if (callee.isHostFunction()) {
            result_ = call_host_function(callee.asHostFunction(), expr.args, expr.diagnostic_token);
            return;
        }

        fail(expr.diagnostic_token, "Value is not callable: " + callee.toString());
    }

    void visit(const IndexExpr& expr) override {
        Value target = evaluate(*expr.target);
        if (!target.isList()) {
            fail(expr.diagnostic_token, "Only list indexing is supported for now");
        }
        if (expr.args.size() != 1 || expr.args[0].name.has_value()) {
            fail(expr.diagnostic_token, "List indexing requires exactly one positional argument");
        }
        Value index_val = evaluate(*expr.args[0].value);
        if (!index_val.isInt()) {
            fail(expr.diagnostic_token, "List index must be an integer");
        }
        int64_t index = index_val.asInt();
        const auto& list_elems = target.asList();
        if (index < 0 || index >= static_cast<int64_t>(list_elems.size())) {
            fail(expr.diagnostic_token, "List index out of bounds: " + std::to_string(index));
        }
        result_ = list_elems[index];
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
            if (pattern.getType()->hasSetness()) {
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
        if (const auto* intrinsic = dynamic_cast<const IntrinsicExpr*>(stmt.expression.get());
            intrinsic != nullptr && is_harness_intrinsic(intrinsic->name)) {
            if (is_name(intrinsic->name, "`expect_test_failure")) {
                expectations.has_expectations = true;
                expectations.expect_test_failure = true;
            }
            return;
        }
        evaluate(*stmt.expression);
    }

    void visit(const LetStmt& stmt) override {
        Value initializer = evaluate(*stmt.binding.initializer);
        Value constraint = stmt.binding.type_bound ? evaluate(*stmt.binding.type_bound) : Any();
        enforce_constraint(constraint, initializer, stmt.diagnostic_token);

        auto binding = std::make_shared<Binding>(constraint, constraint, initializer, stmt.binding.is_final);
        define_binding(stmt.binding.name.lexeme, std::move(binding), stmt.binding.name);
    }

    void visit(const BreakStmt& stmt) override {
        Value value = stmt.value ? evaluate(*stmt.value) : VoidVal();
        throw BreakSignal{std::move(value)};
    }

    void visit(const AssignStmt& stmt) override {
        const auto* target = dynamic_cast<const IdentifierExpr*>(stmt.target.get());
        if (target == nullptr) {
            fail(stmt.diagnostic_token, "Only identifier assignment is supported yet");
        }

        auto binding = lookup_binding(target->name, target->diagnostic_token);
        Value value = evaluate(*stmt.value);

        switch (stmt.op.type) {
            case token_type::equal:
                break;
            case token_type::plus_equal:
                value = Value::make_int(as_int(binding->getCV(), stmt.op) + as_int(value, stmt.op));
                break;
            case token_type::minus_equal:
                value = Value::make_int(as_int(binding->getCV(), stmt.op) - as_int(value, stmt.op));
                break;
            case token_type::star_equal:
                value = Value::make_int(as_int(binding->getCV(), stmt.op) * as_int(value, stmt.op));
                break;
            case token_type::slash_equal: {
                int64_t rhs = as_int(value, stmt.op);
                if (rhs == 0) fail(stmt.op, "Division by zero");
                value = Value::make_int(as_int(binding->getCV(), stmt.op) / rhs);
                break;
            }
            case token_type::percent_equal: {
                int64_t rhs = as_int(value, stmt.op);
                if (rhs == 0) fail(stmt.op, "Modulo by zero");
                value = Value::make_int(as_int(binding->getCV(), stmt.op) % rhs);
                break;
            }
            default:
                fail(stmt.op, "Unsupported assignment operator");
        }

        enforce_constraint(binding->getFC(), value, stmt.diagnostic_token);
        binding->setCV(std::move(value));
    }

    void visit(const IfStmt& stmt) override {
        if (as_bool(evaluate(*stmt.condition), stmt.diagnostic_token)) {
            execute_stmt(*stmt.then_branch);
        } else if (stmt.else_branch) {
            execute_stmt(*stmt.else_branch);
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
    explicit Impl(std::ostream& out) : evaluator(out) {}

    SessionExpectations getExpectations() const {
        return evaluator.expectations;
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
        evaluator.execute(stmts);
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
                evaluator.execute_boot(loaded->stmts);
            } else {
                evaluator.execute(loaded->stmts);
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

private:
    Evaluator evaluator;
    std::vector<std::unique_ptr<SourceUnit>> sources;
};

Session::Session(std::ostream& out) : impl_(std::make_unique<Impl>(out)) {}
Session::~Session() = default;
Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&&) noexcept = default;

void Session::execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
    impl_->execute(stmts);
}

void Session::execute_source(std::string source, std::string label) {
    impl_->execute_source(std::move(source), std::move(label), false);
}

void Session::execute_boot_source(std::string source, std::string label) {
    impl_->execute_source(std::move(source), std::move(label), true);
}

SessionExpectations Session::getExpectations() const {
    return impl_->getExpectations();
}

void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::ostream& out) {
    Session session(out);
    session.execute(stmts);
}

} // namespace chirp::interpreter
