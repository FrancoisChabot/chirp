#include "compiler.h"

#include "opcodes.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <set>
#include <iostream>

namespace chirp::vm {

namespace {

struct VariableRef {
    enum class Kind {
        Local,
        Capture,
        Global,
    };

    Kind kind;
    uint32_t index = 0;
};

class CompilerContext {
public:
    CompilerContext* parent = nullptr;
    bool is_root = false;
    uint32_t next_local = 0;
    std::vector<CaptureSource> captures;
    std::unordered_map<std::string, uint32_t> capture_indices;

    explicit CompilerContext(CompilerContext* parent_context = nullptr, bool root = false)
        : parent(parent_context), is_root(root) {}

    uint32_t allocateLocal() {
        return next_local++;
    }

    std::optional<uint32_t> findCapture(const std::string& name) const {
        auto found = capture_indices.find(name);
        if (found == capture_indices.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    uint32_t ensureCapture(const std::string& name, CaptureSource source) {
        if (auto existing = findCapture(name)) {
            return *existing;
        }

        uint32_t index = static_cast<uint32_t>(captures.size());
        captures.push_back(source);
        capture_indices.emplace(name, index);
        return index;
    }
};

class CompilerEnvironment {
public:
    CompilerEnvironment* parent = nullptr;
    CompilerContext* context = nullptr;
    bool top_level_scope = false;
    struct LocalInfo {
        uint32_t slot;
        bool is_final;
        std::optional<uint32_t> constraint_slot;
        bool is_pure = false;
    };
    std::unordered_map<std::string, LocalInfo> locals;

    CompilerEnvironment(CompilerContext* current_context, CompilerEnvironment* parent_env = nullptr, bool top_level = false)
        : parent(parent_env), context(current_context), top_level_scope(top_level) {}

    bool isFinal(const std::string& name, const std::unordered_set<std::string>* global_finals) {
        if (auto local = locals.find(name); local != locals.end()) {
            return local->second.is_final;
        }
        if (parent) {
            return parent->isFinal(name, global_finals);
        }
        if (global_finals) {
            return global_finals->count(name) > 0;
        }
        return false;
    }

    VariableRef resolve(const std::string& name) {
        if (auto local = locals.find(name); local != locals.end()) {
            return {VariableRef::Kind::Local, local->second.slot};
        }

        if (auto capture = context->findCapture(name)) {
            return {VariableRef::Kind::Capture, *capture};
        }

        if (parent == nullptr) {
            return {VariableRef::Kind::Global, 0};
        }

        if (parent->context == context) {
            return parent->resolve(name);
        }

        VariableRef parent_ref = parent->resolve(name);
        if (parent_ref.kind == VariableRef::Kind::Global) {
            return parent_ref;
        }

        CaptureSource source{
            parent_ref.kind == VariableRef::Kind::Local ? CaptureSourceKind::Local : CaptureSourceKind::Capture,
            parent_ref.index,
        };
        return {VariableRef::Kind::Capture, context->ensureCapture(name, source)};
    }
};

    class PurityVisitor : public frontend::ASTVisitor, public frontend::StmtVisitor {
    public:
        const CompilerEnvironment* env;
        const std::unordered_map<std::string, bool>& global_purity;
        const std::vector<frontend::NamedBinding>& params;
        std::set<std::string> local_vars;
        bool is_pure = true;
        std::string current_lambda_name;

        PurityVisitor(const CompilerEnvironment* e,
                      const std::unordered_map<std::string, bool>& gp,
                      const std::vector<frontend::NamedBinding>& p,
                      const std::string& current_name)
            : env(e), global_purity(gp), params(p), current_lambda_name(current_name) {}

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

            if (!current_lambda_name.empty() && name == current_lambda_name) {
                for (const auto& arg : expr.args) {
                    if (arg.value) arg.value->accept(*this);
                }
                return;
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

            if (name == "print" || name == "io.print" || name == "`print" ||
                name == "io.write" || name == "`write" ||
                name == "io.input" || name == "`input" ||
                name == "system.register" || name == "`register" ||
                name == "memory.heap_create" || name == "`heap_create" ||
                name == "memory.heap_destroy" || name == "`heap_destroy" ||
                name == "memory.heap_shared_create" || name == "`heap_shared_create" ||
                name == "memory.heap_shared_destroy" || name == "`heap_shared_destroy" ||
                name == "system.exit" || name == "`exit") {
                is_pure = false; return;
            }

            auto gp_it = global_purity.find(name);
            if (gp_it != global_purity.end()) {
                if (!gp_it->second) {
                    is_pure = false;
                    return;
                }
            } else {
                bool found = false;
                const CompilerEnvironment* curr = env;
                while (curr) {
                    auto it = curr->locals.find(name);
                    if (it != curr->locals.end()) {
                        found = true;
                        if (!it->second.is_pure) {
                            is_pure = false;
                        }
                        break;
                    }
                    curr = curr->parent;
                }
                if (!found) {
                    if (name != "is_pure" && name != "compute.is_pure" && name != "`is_pure" &&
                        name != "import" && name != "`import" &&
                        name != "values.same" && name != "`same" &&
                        name != "types.type_of" && name != "`type_of" &&
                        name != "traits.implements" && name != "`implements" &&
                        name != "sets.types_in_set" && name != "`types_in_set" &&
                        name != "sets.enumerable" && name != "`is_enumerable" &&
                        name != "sets.coextensive" && name != "`coextensive" &&
                        name != "compute.lambda_param_space" && name != "`lambda_param_space" &&
                        name != "compute.lambda_result_space" && name != "`lambda_result_space" &&
                        name != "types.construction_args" && name != "`construction_args" &&
                        name != "types.construct" && name != "`construct" &&
                        name != "types.mint_finite" && name != "`mint_finite" &&
                        name != "types.is_struct_type" && name != "`is_struct_type" &&
                        name != "traits.make" && name != "`make_trait" &&
                        name != "traits.interface" && name != "`interface_of" &&
                        name != "traits.implement" && name != "`implement" &&
                        name != "traits.implementation" && name != "`implementation" &&
                        name != "int" && name != "`int" &&
                        name != "bool" && name != "`bool" &&
                        name != "string" && name != "`string" &&
                        name != "char" && name != "`char" &&
                        name != "symbol" && name != "`symbol" &&
                        name != "any" && name != "`any" &&
                        name != "type" && name != "`type" &&
                        name != "lambda" && name != "`lambda" &&
                        name != "trait" && name != "`trait" &&
                        name != "true" && name != "`true" &&
                        name != "false" && name != "`false") {
                        is_pure = false; return;
                    }
                }
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
            if (!is_pure) return;
            if (expr.left) expr.left->accept(*this);
            if (expr.right) expr.right->accept(*this);
        }
        void visit(const frontend::ListExpr& expr) override {
            if (!is_pure) return;
            for (const auto& e : expr.elements) {
                if (e) e->accept(*this);
            }
        }
        void visit(const frontend::LambdaExpr& expr) override {
            if (!is_pure) return;
            for (const auto& param : expr.parameters) {
                if (param.type_bound) param.type_bound->accept(*this);
            }
            if (expr.return_bound) expr.return_bound->accept(*this);
        }
        void visit(const frontend::SignatureExpr& expr) override {
            if (!is_pure) return;
            for (const auto& param : expr.parameters) {
                if (param.type_bound) param.type_bound->accept(*this);
            }
            if (expr.return_bound) expr.return_bound->accept(*this);
        }
        void visit(const frontend::StructExpr& expr) override {
            if (!is_pure) return;
            for (const auto& f : expr.fields) {
                if (f.initializer) f.initializer->accept(*this);
            }
        }
        void visit(const frontend::ConstructedSetExpr& expr) override {
            if (!is_pure) return;
            local_vars.insert(std::string(expr.binding.name.lexeme));
            if (expr.binding.initializer) expr.binding.initializer->accept(*this);
            if (expr.binding.type_bound) expr.binding.type_bound->accept(*this);
            if (expr.condition) expr.condition->accept(*this);
        }
        void visit(const frontend::AnonymousStructLiteralExpr& expr) override {
            if (!is_pure) return;
            for (const auto& field : expr.fields) {
                if (field.value) field.value->accept(*this);
            }
        }
        void visit(const frontend::BlockExpr& expr) override {
            if (!is_pure) return;
            for (const auto& s : expr.statements) {
                if (s) s->accept(*this);
            }
        }
        void visit(const frontend::IfStmt& stmt) override {
            if (!is_pure) return;
            if (stmt.condition) stmt.condition->accept(*this);
            if (stmt.then_branch) stmt.then_branch->accept(*this);
            if (stmt.else_branch) stmt.else_branch->accept(*this);
        }
        void visit(const frontend::ExprStmt& stmt) override {
            if (!is_pure) return;
            if (stmt.expression) stmt.expression->accept(*this);
        }
        void visit(const frontend::BreakStmt&) override {}
        void visit(const frontend::DebugStmt&) override {}
        void visit(const frontend::IntrinsicExpr&) override {}
        void visit(const frontend::UnaryExpr& expr) override {
            if (!is_pure) return;
            if (expr.right) expr.right->accept(*this);
        }
        void visit(const frontend::GroupingExpr& expr) override {
            if (!is_pure) return;
            if (expr.expression) expr.expression->accept(*this);
        }
        void visit(const frontend::NumberExpr&) override {}
        void visit(const frontend::StringExpr&) override {}
        void visit(const frontend::FStringExpr& expr) override {
            if (!is_pure) return;
            for (const auto& part : expr.parts) {
                if (part) part->accept(*this);
            }
        }
        void visit(const frontend::CharExpr&) override {}
        void visit(const frontend::SymbolicConstantExpr&) override {}
        void visit(const frontend::EnumExpr&) override {}
        void visit(const frontend::EnumeratedSetExpr& expr) override {
            if (!is_pure) return;
            for (const auto& e : expr.elements) {
                if (e) e->accept(*this);
            }
        }
        void visit(const frontend::IfExpr& expr) override {
            if (!is_pure) return;
            if (expr.condition) expr.condition->accept(*this);
            if (expr.then_branch) expr.then_branch->accept(*this);
            if (expr.else_branch) expr.else_branch->accept(*this);
        }
        void visit(const frontend::WhileExpr& expr) override {
            if (!is_pure) return;
            if (expr.condition) expr.condition->accept(*this);
            if (expr.body) expr.body->accept(*this);
        }
        void visit(const frontend::IndexExpr& expr) override {
            if (!is_pure) return;
            if (expr.target) expr.target->accept(*this);
            for (const auto& a : expr.args) {
                if (a.value) a.value->accept(*this);
            }
        }
        void visit(const frontend::MatchExpr& expr) override {
            if (!is_pure) return;
            if (expr.subject) expr.subject->accept(*this);
            for (const auto& arm : expr.arms) {
                if (arm.body) arm.body->accept(*this);
            }
        }
    };

    bool determinePurity(const frontend::LambdaExpr& lambda,
                         const CompilerEnvironment* env,
                         const std::unordered_map<std::string, bool>& global_purity,
                         const std::string& current_name) {
        PurityVisitor visitor(env, global_purity, lambda.parameters, current_name);
        if (lambda.body) {
            lambda.body->accept(visitor);
        }
        return visitor.is_pure;
    }

class CompilerVisitor : public frontend::ASTVisitor, public frontend::StmtVisitor {
public:
    struct CleanupScope {
        std::vector<VariableRef> bindings;
    };

    std::shared_ptr<ProgramUnit> unit;
    CompilerEnvironment* env;
    std::vector<CleanupScope> cleanup_scopes;
    std::vector<size_t> break_targets;
    std::unordered_set<std::string>* global_final_bindings;
    std::unordered_map<std::string, bool>* global_purity;
    std::string current_binding_name;
    std::vector<std::string> public_globals;

    CompilerVisitor(std::shared_ptr<ProgramUnit> current_unit, CompilerEnvironment* current_env,
                    std::unordered_set<std::string>* global_finals = nullptr,
                    std::unordered_map<std::string, bool>* global_purity_map = nullptr)
        : unit(std::move(current_unit)), env(current_env), global_final_bindings(global_finals), global_purity(global_purity_map) {}

    void compileStatements(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
        for (const auto& stmt : stmts) {
            stmt->accept(*this);
        }
    }

    void emitU32(uint32_t value) {
        for (int i = 0; i < 4; ++i) {
            unit->emit((value >> (i * 8)) & 0xFF);
        }
    }

    void emitU64(uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            unit->emit((value >> (i * 8)) & 0xFF);
        }
    }

    void emitStringIndex(const std::string& text) {
        emitU32(unit->addStringConstant(text));
    }

    void emitBoolOperand(bool value) {
        unit->emit(static_cast<uint8_t>(OperandType::ImmBool));
        unit->emit(value ? 1 : 0);
    }

    void emitIntegerLiteralOperand(const std::string& text, bool negate = false) {
        unit->emit(static_cast<uint8_t>(OperandType::ImmInt));
        emitU32(unit->addIntConstant(BigInt(negate ? "-" + text : text)));
    }

    std::shared_ptr<ProgramUnit> compileExpressionUnit(const frontend::Expr& expr) {
        auto expr_unit = std::make_shared<ProgramUnit>();
        CompilerContext expr_context(env->context);
        CompilerEnvironment expr_env(&expr_context, env, false);
        CompilerVisitor expr_visitor(expr_unit, &expr_env, global_final_bindings);
        expr_unit->emit(encodeInstruction(Opcode::Return, Domain::Generic));
        expr_visitor.emitOperand(expr);
        expr_unit->num_locals = expr_context.next_local;
        expr_unit->captures = expr_context.captures;
        return expr_unit;
    }

    std::shared_ptr<ProgramUnit> compilePredicateUnit(const frontend::NamedBinding& binding, const frontend::Expr& expr) {
        auto predicate_unit = std::make_shared<ProgramUnit>();
        CompilerContext predicate_context(env->context);
        CompilerEnvironment predicate_env(&predicate_context, env, false);
        std::string name(binding.name.lexeme);
        if (env->isFinal(name, global_final_bindings)) {
            throw std::runtime_error("Identifier '" + name + "' cannot shadow final binding");
        }
        predicate_unit->parameter_names.push_back(name);
        predicate_env.locals.emplace(name, CompilerEnvironment::LocalInfo{predicate_context.allocateLocal(), binding.is_final});
        CompilerVisitor predicate_visitor(predicate_unit, &predicate_env, global_final_bindings);
        predicate_unit->emit(encodeInstruction(Opcode::Return, Domain::Generic));
        predicate_visitor.emitOperand(expr);
        predicate_unit->num_locals = predicate_context.next_local;
        predicate_unit->captures = predicate_context.captures;
        return predicate_unit;
    }

    void emitResolvedVariableOperand(const std::string& name) {
        VariableRef ref = env->resolve(name);
        emitVariableOperand(ref, name);
    }

    void emitVariableOperand(const VariableRef& ref, const std::string& global_name = "") {
        switch (ref.kind) {
            case VariableRef::Kind::Local:
                unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                emitU32(ref.index);
                break;
            case VariableRef::Kind::Capture:
                unit->emit(static_cast<uint8_t>(OperandType::Capture));
                emitU32(ref.index);
                break;
            case VariableRef::Kind::Global:
                unit->emit(static_cast<uint8_t>(OperandType::Identifier));
                emitStringIndex(global_name);
                break;
        }
    }

    void emitDropBinding(const VariableRef& ref, const std::string& global_name = "") {
        unit->emit(encodeInstruction(Opcode::DropBinding, Domain::Generic));
        emitVariableOperand(ref, global_name);
    }

    void recordCleanupBinding(const VariableRef& ref) {
        if (cleanup_scopes.empty()) {
            return;
        }
        cleanup_scopes.back().bindings.push_back(ref);
    }

    static bool sameBinding(const VariableRef& left, const VariableRef& right) {
        return left.kind == right.kind && left.index == right.index;
    }

    std::optional<VariableRef> terminalBindingFromBreakValue(const frontend::BreakStmt& stmt) {
        if (!stmt.value) {
            return std::nullopt;
        }
        if (auto* ident = dynamic_cast<const frontend::IdentifierExpr*>(stmt.value.get())) {
            return env->resolve(std::string(ident->name));
        }
        return std::nullopt;
    }

    void emitCleanupScope(const CleanupScope& scope, const std::optional<VariableRef>& terminal = std::nullopt) {
        for (auto it = scope.bindings.rbegin(); it != scope.bindings.rend(); ++it) {
            if (terminal.has_value() && sameBinding(*it, *terminal)) {
                continue;
            }
            emitDropBinding(*it);
        }
    }

    void emitCleanupFromBase(size_t base, const std::optional<VariableRef>& terminal = std::nullopt) {
        for (size_t i = cleanup_scopes.size(); i > base; --i) {
            emitCleanupScope(cleanup_scopes[i - 1], terminal);
        }
    }

    std::vector<VariableRef> collectCleanupBindingsFromBase(size_t base, const std::optional<VariableRef>& terminal = std::nullopt) {
        std::vector<VariableRef> bindings;
        for (size_t i = cleanup_scopes.size(); i > base; --i) {
            const auto& scope = cleanup_scopes[i - 1];
            for (auto it = scope.bindings.rbegin(); it != scope.bindings.rend(); ++it) {
                if (terminal.has_value() && sameBinding(*it, *terminal)) {
                    continue;
                }
                bindings.push_back(*it);
            }
        }
        return bindings;
    }

    void emitBreakInstruction(const frontend::Expr* value_expr,
                              size_t cleanup_base,
                              const std::optional<VariableRef>& terminal = std::nullopt) {
        unit->emit(encodeInstruction(Opcode::Break, Domain::Generic));
        if (value_expr) {
            emitOperand(*value_expr);
        } else {
            unit->emit(static_cast<uint8_t>(OperandType::ImmNull));
        }
        auto cleanup_bindings = collectCleanupBindingsFromBase(cleanup_base, terminal);
        emitU32(static_cast<uint32_t>(cleanup_bindings.size()));
        for (const auto& binding : cleanup_bindings) {
            emitVariableOperand(binding);
        }
    }

    void emitOperand(const frontend::Expr& expr) {
        if (auto grouping = dynamic_cast<const frontend::GroupingExpr*>(&expr)) {
            emitOperand(*grouping->expression);
        } else if (auto num = dynamic_cast<const frontend::NumberExpr*>(&expr)) {
            emitIntegerLiteralOperand(std::string(num->value));
        } else if (auto unary = dynamic_cast<const frontend::UnaryExpr*>(&expr)) {
            if (unary->op == frontend::UnaryOp::Negate) {
                if (auto num = dynamic_cast<const frontend::NumberExpr*>(unary->right.get())) {
                    emitIntegerLiteralOperand(std::string(num->value), true);
                    return;
                }
            }
            unit->emit(static_cast<uint8_t>(OperandType::Inline));
            expr.accept(*this);
        } else if (auto ident = dynamic_cast<const frontend::IdentifierExpr*>(&expr)) {
            emitResolvedVariableOperand(std::string(ident->name));
        } else if (auto intrinsic = dynamic_cast<const frontend::IntrinsicExpr*>(&expr)) {
            emitResolvedVariableOperand(std::string(intrinsic->name));
        } else if (auto str = dynamic_cast<const frontend::StringExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmString));
            frontend::token_type t = str->diagnostic_token.type;
            if (t == frontend::token_type::fstring_head || t == frontend::token_type::fstring_middle || t == frontend::token_type::fstring_tail || t == frontend::token_type::fstring_literal) {
                emitStringIndex(frontend::decode_fstring_part(std::string(str->value), t));
            } else {
                emitStringIndex(frontend::decode_quoted_literal(std::string(str->value)));
            }
        } else if (auto chr = dynamic_cast<const frontend::CharExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmChar));
            std::string decoded = frontend::decode_quoted_literal(std::string(chr->value));
            uint32_t value = frontend::decode_utf8_char(decoded);
            emitU32(value);
        } else if (auto sym = dynamic_cast<const frontend::SymbolicConstantExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmSymbol));
            emitStringIndex(std::string(sym->value));
        } else {
            unit->emit(static_cast<uint8_t>(OperandType::Inline));
            expr.accept(*this);
        }
    }

    void visit(const frontend::ExprStmt& stmt) override {
        unit->emit(encodeInstruction(Opcode::Eval, Domain::Generic));
        emitOperand(*stmt.expression);
    }

    void visit(const frontend::LetStmt& stmt) override {
        if (!stmt.binding.initializer) {
            throw std::runtime_error("LetStmt without initializer is not supported in the VM");
        }

        std::string name(stmt.binding.name.lexeme);

        if (env->locals.find(name) != env->locals.end()) {
            throw std::runtime_error("Identifier '" + name + "' is already defined in this scope");
        }

        if (env->isFinal(name, global_final_bindings)) {
            throw std::runtime_error("Identifier '" + name + "' cannot shadow final binding");
        }

        std::string old_binding_name = current_binding_name;
        current_binding_name = name;

        bool is_initializer_pure = false;
        std::unordered_map<std::string, bool> dummy_purity;
        if (auto lambda = dynamic_cast<const frontend::LambdaExpr*>(stmt.binding.initializer.get())) {
            is_initializer_pure = determinePurity(*lambda, env, global_purity ? *global_purity : dummy_purity, name);
        } else {
            PurityVisitor visitor(env, global_purity ? *global_purity : dummy_purity, {}, name);
            stmt.binding.initializer->accept(visitor);
            is_initializer_pure = visitor.is_pure;
        }

        std::optional<uint32_t> constraint_slot;
        if (stmt.binding.type_bound) {
            if (env->top_level_scope) {
                unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));
                unit->emit(static_cast<uint8_t>(OperandType::Identifier));
                emitStringIndex("__chirp_constraint_" + name);
                emitOperand(*stmt.binding.type_bound);
            } else {
                constraint_slot = env->context->allocateLocal();
                unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));
                unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                emitU32(*constraint_slot);
                emitOperand(*stmt.binding.type_bound);
            }
        }

        unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));

        if (env->top_level_scope) {
            if (stmt.binding.is_final && global_final_bindings) {
                global_final_bindings->insert(name);
            }
            if (global_purity) {
                (*global_purity)[name] = is_initializer_pure;
            }
            if (stmt.is_public) {
                public_globals.push_back(name);
            }
            unit->emit(static_cast<uint8_t>(OperandType::Identifier));
            emitStringIndex(name);
        } else {
            uint32_t slot = env->context->allocateLocal();
            unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
            emitU32(slot);
            env->locals.emplace(name, CompilerEnvironment::LocalInfo{slot, stmt.binding.is_final, constraint_slot, is_initializer_pure});
            recordCleanupBinding({VariableRef::Kind::Local, slot});
        }

        if (stmt.binding.type_bound) {
            unit->emit(static_cast<uint8_t>(OperandType::Inline));
            if (env->top_level_scope) {
                unit->emit(encodeInstruction(Opcode::EnforceGlobalConstraint, Domain::Generic));
                emitStringIndex(name);
            } else {
                unit->emit(encodeInstruction(Opcode::EnforceConstraint, Domain::Generic));
                unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                emitU32(*constraint_slot);
            }
            emitOperand(*stmt.binding.initializer);
        } else {
            emitOperand(*stmt.binding.initializer);
        }

        current_binding_name = old_binding_name;
    }

    void visit(const frontend::BreakStmt& stmt) override {
        if (break_targets.empty()) {
            throw std::runtime_error("BreakStmt outside of a breakable region is not supported in the VM");
        }
        std::optional<VariableRef> terminal = terminalBindingFromBreakValue(stmt);
        emitBreakInstruction(stmt.value.get(), break_targets.back(), terminal);
    }

    void visit(const frontend::AssignStmt& stmt) override {
        auto emitRHS = [&]() {
            if (stmt.op.type != frontend::token_type::equal) {
                unit->emit(static_cast<uint8_t>(OperandType::Inline));
                switch (stmt.op.type) {
                    case frontend::token_type::plus_equal: unit->emit(encodeInstruction(Opcode::Add, Domain::Generic)); break;
                    case frontend::token_type::minus_equal: unit->emit(encodeInstruction(Opcode::Sub, Domain::Generic)); break;
                    case frontend::token_type::star_equal: unit->emit(encodeInstruction(Opcode::Mul, Domain::Generic)); break;
                    case frontend::token_type::slash_equal: unit->emit(encodeInstruction(Opcode::Div, Domain::Generic)); break;
                    case frontend::token_type::percent_equal: unit->emit(encodeInstruction(Opcode::Mod, Domain::Generic)); break;
                    default: throw std::runtime_error("Unsupported assignment operator");
                }
                emitOperand(*stmt.target);
                emitOperand(*stmt.value);
            } else {
                emitOperand(*stmt.value);
            }
        };

        if (auto* ident = dynamic_cast<const frontend::IdentifierExpr*>(stmt.target.get())) {
            std::string name(ident->name);
            VariableRef ref = env->resolve(name);

            if (env->isFinal(name, global_final_bindings)) {
                throw std::runtime_error("Cannot assign to final identifier '" + name + "'");
            }

            unit->emit(encodeInstruction(Opcode::Assign, Domain::Generic));

            bool has_constraint = false;
            std::optional<uint32_t> constraint_slot;
            bool is_global = false;

            if (ref.kind == VariableRef::Kind::Local) {
                auto local = env->locals.find(name);
                if (local != env->locals.end() && local->second.constraint_slot) {
                    has_constraint = true;
                    constraint_slot = local->second.constraint_slot;
                }
            } else if (ref.kind == VariableRef::Kind::Global) {
                has_constraint = true;
                is_global = true;
            }

            switch (ref.kind) {
                case VariableRef::Kind::Local:
                    unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                    emitU32(ref.index);
                    break;
                case VariableRef::Kind::Capture:
                    unit->emit(static_cast<uint8_t>(OperandType::Capture));
                    emitU32(ref.index);
                    break;
                case VariableRef::Kind::Global:
                    unit->emit(static_cast<uint8_t>(OperandType::Identifier));
                    emitStringIndex(name);
                    break;
            }

            if (has_constraint) {
                unit->emit(static_cast<uint8_t>(OperandType::Inline));
                if (is_global) {
                    unit->emit(encodeInstruction(Opcode::EnforceGlobalConstraint, Domain::Generic));
                    emitStringIndex(name);
                } else {
                    unit->emit(encodeInstruction(Opcode::EnforceConstraint, Domain::Generic));
                    unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                    emitU32(*constraint_slot);
                }
                emitRHS();
            } else {
                emitRHS();
            }
            return;
        }

        if (auto* unary = dynamic_cast<const frontend::UnaryExpr*>(stmt.target.get())) {
            if (unary->op == frontend::UnaryOp::Deref) {
                unit->emit(encodeInstruction(Opcode::StoreDeref, Domain::Generic));
                emitOperand(*unary->right);
                emitRHS();
                return;
            }
        }

        throw std::runtime_error("AssignStmt target is not supported in the VM");
    }

    void visit(const frontend::IfStmt& stmt) override {
        unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
        emitOperand(*stmt.condition);

        size_t true_len_offset = unit->bytecode.size();
        emitU32(0);
        size_t true_start = unit->bytecode.size();
        unit->emit(static_cast<uint8_t>(OperandType::Inline));
        stmt.then_branch->accept(*this);
        uint32_t true_len = static_cast<uint32_t>(unit->bytecode.size() - true_start);
        for (int i = 0; i < 4; ++i) {
            unit->bytecode[true_len_offset + i] = static_cast<uint8_t>((true_len >> (i * 8)) & 0xFF);
        }

        size_t false_len_offset = unit->bytecode.size();
        emitU32(0);
        size_t false_start = unit->bytecode.size();
        if (stmt.else_branch) {
            unit->emit(static_cast<uint8_t>(OperandType::Inline));
            stmt.else_branch->accept(*this);
        } else {
            unit->emit(static_cast<uint8_t>(OperandType::ImmNull));
        }
        uint32_t false_len = static_cast<uint32_t>(unit->bytecode.size() - false_start);
        for (int i = 0; i < 4; ++i) {
            unit->bytecode[false_len_offset + i] = static_cast<uint8_t>((false_len >> (i * 8)) & 0xFF);
        }
    }

    void visit(const frontend::DebugStmt& stmt) override {
        unit->emit(encodeInstruction(Opcode::Block, Domain::Generic));
        emitU32(static_cast<uint32_t>(stmt.statements.size()));
        break_targets.push_back(cleanup_scopes.size());
        try {
            for (const auto& s : stmt.statements) {
                s->accept(*this);
            }
        } catch (...) {
            break_targets.pop_back();
            throw;
        }
        break_targets.pop_back();
    }

    void visit(const frontend::BinaryExpr& expr) override {
        switch (expr.op) {
            case frontend::BinaryOp::Add:
            case frontend::BinaryOp::Sub:
            case frontend::BinaryOp::Mul:
            case frontend::BinaryOp::Div:
            case frontend::BinaryOp::Mod:
                switch (expr.op) {
                    case frontend::BinaryOp::Add: unit->emit(encodeInstruction(Opcode::Add, Domain::Generic)); break;
                    case frontend::BinaryOp::Sub: unit->emit(encodeInstruction(Opcode::Sub, Domain::Generic)); break;
                    case frontend::BinaryOp::Mul: unit->emit(encodeInstruction(Opcode::Mul, Domain::Generic)); break;
                    case frontend::BinaryOp::Div: unit->emit(encodeInstruction(Opcode::Div, Domain::Generic)); break;
                    case frontend::BinaryOp::Mod: unit->emit(encodeInstruction(Opcode::Mod, Domain::Generic)); break;
                    default: break;
                }
                emitOperand(*expr.left);
                emitOperand(*expr.right);
                return;
            case frontend::BinaryOp::Dot:
                unit->emit(encodeInstruction(Opcode::GetField, Domain::Generic));
                emitOperand(*expr.left);
                if (auto ident = dynamic_cast<const frontend::IdentifierExpr*>(expr.right.get())) {
                    unit->emit(static_cast<uint8_t>(OperandType::ImmString));
                    emitStringIndex(std::string(ident->name));
                    return;
                }
                throw std::runtime_error("Dot right-hand side must be an identifier");
            case frontend::BinaryOp::And:
                unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
                emitOperand(*expr.left);
                {
                    size_t true_len_offset = unit->bytecode.size();
                    emitU32(0);
                    size_t true_start = unit->bytecode.size();
                    emitOperand(*expr.right);
                    uint32_t true_len = static_cast<uint32_t>(unit->bytecode.size() - true_start);
                    for (int i = 0; i < 4; ++i) {
                        unit->bytecode[true_len_offset + i] = static_cast<uint8_t>((true_len >> (i * 8)) & 0xFF);
                    }

                    size_t false_len_offset = unit->bytecode.size();
                    emitU32(0);
                    size_t false_start = unit->bytecode.size();
                    emitBoolOperand(false);
                    uint32_t false_len = static_cast<uint32_t>(unit->bytecode.size() - false_start);
                    for (int i = 0; i < 4; ++i) {
                        unit->bytecode[false_len_offset + i] = static_cast<uint8_t>((false_len >> (i * 8)) & 0xFF);
                    }
                }
                return;
            case frontend::BinaryOp::Or:
                unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
                emitOperand(*expr.left);
                {
                    size_t true_len_offset = unit->bytecode.size();
                    emitU32(0);
                    size_t true_start = unit->bytecode.size();
                    emitBoolOperand(true);
                    uint32_t true_len = static_cast<uint32_t>(unit->bytecode.size() - true_start);
                    for (int i = 0; i < 4; ++i) {
                        unit->bytecode[true_len_offset + i] = static_cast<uint8_t>((true_len >> (i * 8)) & 0xFF);
                    }

                    size_t false_len_offset = unit->bytecode.size();
                    emitU32(0);
                    size_t false_start = unit->bytecode.size();
                    emitOperand(*expr.right);
                    uint32_t false_len = static_cast<uint32_t>(unit->bytecode.size() - false_start);
                    for (int i = 0; i < 4; ++i) {
                        unit->bytecode[false_len_offset + i] = static_cast<uint8_t>((false_len >> (i * 8)) & 0xFF);
                    }
                }
                return;
            case frontend::BinaryOp::In:
                unit->emit(encodeInstruction(Opcode::Contains, Domain::Generic));
                emitOperand(*expr.left);
                emitOperand(*expr.right);
                return;
            case frontend::BinaryOp::NotIn:
                unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
                unit->emit(static_cast<uint8_t>(OperandType::Inline));
                unit->emit(encodeInstruction(Opcode::Contains, Domain::Generic));
                emitOperand(*expr.left);
                emitOperand(*expr.right);
                {
                    size_t true_len_offset = unit->bytecode.size();
                    emitU32(0);
                    size_t true_start = unit->bytecode.size();
                    emitBoolOperand(false);
                    uint32_t true_len = static_cast<uint32_t>(unit->bytecode.size() - true_start);
                    for (int i = 0; i < 4; ++i) {
                        unit->bytecode[true_len_offset + i] = static_cast<uint8_t>((true_len >> (i * 8)) & 0xFF);
                    }

                    size_t false_len_offset = unit->bytecode.size();
                    emitU32(0);
                    size_t false_start = unit->bytecode.size();
                    emitBoolOperand(true);
                    uint32_t false_len = static_cast<uint32_t>(unit->bytecode.size() - false_start);
                    for (int i = 0; i < 4; ++i) {
                        unit->bytecode[false_len_offset + i] = static_cast<uint8_t>((false_len >> (i * 8)) & 0xFF);
                    }
                }
                return;
            case frontend::BinaryOp::Union:
                unit->emit(encodeInstruction(Opcode::Union, Domain::Generic));
                emitOperand(*expr.left);
                emitOperand(*expr.right);
                return;
            case frontend::BinaryOp::Intersection:
                unit->emit(encodeInstruction(Opcode::Intersect, Domain::Generic));
                emitOperand(*expr.left);
                emitOperand(*expr.right);
                return;
            case frontend::BinaryOp::Range:
            case frontend::BinaryOp::RangeInclusiveEnd:
                unit->emit(encodeInstruction(Opcode::MakeRange, Domain::Generic));
                unit->emit(expr.op == frontend::BinaryOp::RangeInclusiveEnd ? 1 : 0);
                emitOperand(*expr.left);
                emitOperand(*expr.right);
                return;
            case frontend::BinaryOp::Eq:
            case frontend::BinaryOp::Neq:
            case frontend::BinaryOp::Lt:
            case frontend::BinaryOp::Lte:
            case frontend::BinaryOp::Gt:
            case frontend::BinaryOp::Gte:
                unit->emit(encodeInstruction(Opcode::Compare, Domain::Generic));
                switch (expr.op) {
                    case frontend::BinaryOp::Eq: unit->emit(static_cast<uint8_t>(CompareOp::Eq)); break;
                    case frontend::BinaryOp::Neq: unit->emit(static_cast<uint8_t>(CompareOp::Neq)); break;
                    case frontend::BinaryOp::Lt: unit->emit(static_cast<uint8_t>(CompareOp::Lt)); break;
                    case frontend::BinaryOp::Lte: unit->emit(static_cast<uint8_t>(CompareOp::Lte)); break;
                    case frontend::BinaryOp::Gt: unit->emit(static_cast<uint8_t>(CompareOp::Gt)); break;
                    case frontend::BinaryOp::Gte: unit->emit(static_cast<uint8_t>(CompareOp::Gte)); break;
                    default: break;
                }
                emitOperand(*expr.left);
                emitOperand(*expr.right);
                return;
            default:
                throw std::runtime_error("Unsupported binary op in the VM: " + std::to_string(static_cast<int>(expr.op)));
        }
    }

    void visit(const frontend::IfExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
        emitOperand(*expr.condition);

        size_t true_len_offset = unit->bytecode.size();
        emitU32(0);
        size_t true_start = unit->bytecode.size();
        emitOperand(*expr.then_branch);
        uint32_t true_len = static_cast<uint32_t>(unit->bytecode.size() - true_start);
        for (int i = 0; i < 4; ++i) {
            unit->bytecode[true_len_offset + i] = static_cast<uint8_t>((true_len >> (i * 8)) & 0xFF);
        }

        size_t false_len_offset = unit->bytecode.size();
        emitU32(0);
        size_t false_start = unit->bytecode.size();
        emitOperand(*expr.else_branch);
        uint32_t false_len = static_cast<uint32_t>(unit->bytecode.size() - false_start);
        for (int i = 0; i < 4; ++i) {
            unit->bytecode[false_len_offset + i] = static_cast<uint8_t>((false_len >> (i * 8)) & 0xFF);
        }
    }

    void visit(const frontend::LambdaExpr& expr) override {
        auto lambda_unit = std::make_shared<ProgramUnit>();
        std::unordered_map<std::string, bool> dummy_purity;
        lambda_unit->is_pure = determinePurity(expr, env, global_purity ? *global_purity : dummy_purity, current_binding_name);

        CompilerContext lambda_context(env->context);
        CompilerEnvironment lambda_env(&lambda_context, env, false);

        for (const auto& param : expr.parameters) {
            std::string name(param.name.lexeme);
            if (env->isFinal(name, global_final_bindings)) {
                throw std::runtime_error("Identifier '" + name + "' cannot shadow final binding");
            }
            if (lambda_env.locals.find(name) != lambda_env.locals.end()) {
                throw std::runtime_error("Identifier '" + name + "' is already defined in this scope");
            }
            lambda_unit->parameter_names.push_back(name);
            lambda_env.locals.emplace(name, CompilerEnvironment::LocalInfo{lambda_context.allocateLocal(), param.is_final});
        }

        CompilerVisitor lambda_visitor(lambda_unit, &lambda_env, global_final_bindings, global_purity);

        // Compile and enforce return constraint setup
        std::optional<uint32_t> return_constraint_slot;
        if (expr.return_bound) {
            return_constraint_slot = lambda_context.allocateLocal();
            lambda_unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));
            lambda_unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
            lambda_visitor.emitU32(*return_constraint_slot);
            lambda_visitor.emitOperand(*expr.return_bound);
        }

        // Compile and enforce parameter constraints
        for (const auto& param : expr.parameters) {
            if (param.type_bound) {
                std::string name(param.name.lexeme);
                auto local = lambda_env.locals.find(name);
                uint32_t slot = local->second.slot;

                uint32_t constraint_slot = lambda_context.allocateLocal();
                lambda_unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));
                lambda_unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                lambda_visitor.emitU32(constraint_slot);
                lambda_visitor.emitOperand(*param.type_bound);

                lambda_unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));
                lambda_unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                lambda_visitor.emitU32(slot);
                lambda_unit->emit(static_cast<uint8_t>(OperandType::Inline));
                lambda_unit->emit(encodeInstruction(Opcode::EnforceConstraint, Domain::Generic));
                lambda_unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                lambda_visitor.emitU32(constraint_slot);
                lambda_unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                lambda_visitor.emitU32(slot);
            }
        }

        if (expr.return_bound) {
            lambda_unit->emit(encodeInstruction(Opcode::Return, Domain::Generic));
            lambda_unit->emit(static_cast<uint8_t>(OperandType::Inline));
            lambda_unit->emit(encodeInstruction(Opcode::EnforceConstraint, Domain::Generic));
            lambda_unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
            lambda_visitor.emitU32(*return_constraint_slot);
            lambda_visitor.emitOperand(*expr.body);
        } else {
            lambda_unit->emit(encodeInstruction(Opcode::Return, Domain::Generic));
            lambda_visitor.emitOperand(*expr.body);
        }

        lambda_unit->num_locals = lambda_context.next_local;
        lambda_unit->captures = lambda_context.captures;

        unit->emit(encodeInstruction(Opcode::MakeLambda, Domain::Generic));
        emitU32(unit->addChildUnit(lambda_unit));
    }

    void visit(const frontend::CallExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::Call, Domain::Generic));
        emitOperand(*expr.callee);
        emitU32(static_cast<uint32_t>(expr.args.size()));
        for (const auto& arg : expr.args) {
            if (arg.name.has_value()) {
                unit->emit(1);
                emitStringIndex(std::string(arg.name->lexeme));
            } else {
                unit->emit(0);
            }
            emitOperand(*arg.value);
        }
    }

    void visit(const frontend::IdentifierExpr& expr) override {
        throw std::runtime_error("IdentifierExpr should be emitted as an operand");
    }

    void visit(const frontend::NumberExpr& expr) override {
        throw std::runtime_error("NumberExpr should be emitted as an operand");
    }

    void visit(const frontend::UnaryExpr& expr) override {
        switch (expr.op) {
            case frontend::UnaryOp::Not:
                unit->emit(encodeInstruction(Opcode::Not, Domain::Generic));
                emitOperand(*expr.right);
                break;
            case frontend::UnaryOp::Negate:
                unit->emit(encodeInstruction(Opcode::Negate, Domain::Generic));
                emitOperand(*expr.right);
                break;
            case frontend::UnaryOp::Complement:
                unit->emit(encodeInstruction(Opcode::Complement, Domain::Generic));
                emitOperand(*expr.right);
                break;
            case frontend::UnaryOp::Deref:
                unit->emit(encodeInstruction(Opcode::Deref, Domain::Generic));
                emitOperand(*expr.right);
                break;
            default:
                throw std::runtime_error("Unary operation not supported in the VM yet");
        }
    }

    void visit(const frontend::GroupingExpr& expr) override {
        throw std::runtime_error("GroupingExpr should be emitted as an operand");
    }

    void visit(const frontend::StringExpr& expr) override {
        throw std::runtime_error("StringExpr should be emitted as an operand");
    }

    void visit(const frontend::CharExpr& expr) override {
        throw std::runtime_error("CharExpr should be emitted as an operand");
    }

    void visit(const frontend::IntrinsicExpr& expr) override {
        throw std::runtime_error("IntrinsicExpr should be emitted as an operand");
    }

    void visit(const frontend::SymbolicConstantExpr& expr) override {
        throw std::runtime_error("SymbolicConstantExpr should be emitted as an operand");
    }

    void visit(const frontend::EnumeratedSetExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeEnumeratedSet, Domain::Generic));
        emitU32(static_cast<uint32_t>(expr.elements.size()));
        for (const auto& element : expr.elements) {
            emitOperand(*element);
        }
    }

    void visit(const frontend::ConstructedSetExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeConstructedSet, Domain::Generic));
        if (expr.binding.type_bound) {
            unit->emit(1);
            emitU32(unit->addChildUnit(compileExpressionUnit(*expr.binding.type_bound)));
        } else {
            unit->emit(0);
        }
        emitU32(unit->addChildUnit(compilePredicateUnit(expr.binding, *expr.condition)));
    }

    void visit(const frontend::WhileExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::Loop, Domain::Generic));
        
        size_t loop_body_len_offset = unit->bytecode.size();
        emitU32(0);
        size_t loop_start = unit->bytecode.size();
        break_targets.push_back(cleanup_scopes.size());
        try {
            unit->emit(static_cast<uint8_t>(OperandType::Inline));
            unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
            emitOperand(*expr.condition);
            
            size_t true_len_offset = unit->bytecode.size();
            emitU32(0);
            size_t true_start = unit->bytecode.size();
            
            emitOperand(*expr.body);
            
            uint32_t true_len = static_cast<uint32_t>(unit->bytecode.size() - true_start);
            for (int i = 0; i < 4; ++i) {
                unit->bytecode[true_len_offset + i] = static_cast<uint8_t>((true_len >> (i * 8)) & 0xFF);
            }
            
            size_t false_len_offset = unit->bytecode.size();
            emitU32(0);
            size_t false_start = unit->bytecode.size();
            
            unit->emit(static_cast<uint8_t>(OperandType::Inline));
            emitBreakInstruction(nullptr, break_targets.back());
            
            uint32_t false_len = static_cast<uint32_t>(unit->bytecode.size() - false_start);
            for (int i = 0; i < 4; ++i) {
                unit->bytecode[false_len_offset + i] = static_cast<uint8_t>((false_len >> (i * 8)) & 0xFF);
            }
            
            uint32_t loop_len = static_cast<uint32_t>(unit->bytecode.size() - loop_start);
            for (int i = 0; i < 4; ++i) {
                unit->bytecode[loop_body_len_offset + i] = static_cast<uint8_t>((loop_len >> (i * 8)) & 0xFF);
            }
        } catch (...) {
            break_targets.pop_back();
            throw;
        }
        break_targets.pop_back();
    }

    void visit(const frontend::ForExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::ForEach, Domain::Generic));
        emitOperand(*expr.iterable);
        uint32_t slot = env->context->allocateLocal();
        emitU32(slot);
        break_targets.push_back(cleanup_scopes.size());

        std::string name(expr.iterator_binding.name.lexeme);
        if (env->isFinal(name, global_final_bindings)) {
            throw std::runtime_error("Identifier '" + name + "' cannot shadow final binding");
        }

        CompilerEnvironment loop_env(env->context, env, false);
        loop_env.locals.emplace(name, CompilerEnvironment::LocalInfo{slot, expr.iterator_binding.is_final});

        CompilerEnvironment* saved_env = env;
        env = &loop_env;
        try {
            size_t body_len_offset = unit->bytecode.size();
            emitU32(0);
            size_t body_start = unit->bytecode.size();
            emitOperand(*expr.body);
            uint32_t body_len = static_cast<uint32_t>(unit->bytecode.size() - body_start);
            for (int i = 0; i < 4; ++i) {
                unit->bytecode[body_len_offset + i] = static_cast<uint8_t>((body_len >> (i * 8)) & 0xFF);
            }
            env = saved_env;
        } catch (...) {
            env = saved_env;
            break_targets.pop_back();
            throw;
        }
        break_targets.pop_back();
    }

    void visit(const frontend::SignatureExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeSignature, Domain::Generic));
        emitU32(static_cast<uint32_t>(expr.parameters.size()));
        for (const auto& param : expr.parameters) {
            emitStringIndex(std::string(param.name.lexeme));
            if (param.type_bound) {
                unit->emit(1);
                emitU32(unit->addChildUnit(compileExpressionUnit(*param.type_bound)));
            } else {
                unit->emit(0);
            }
        }
        if (expr.return_bound) {
            unit->emit(1);
            emitU32(unit->addChildUnit(compileExpressionUnit(*expr.return_bound)));
        } else {
            unit->emit(0);
        }
    }

    void visit(const frontend::BlockExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::Block, Domain::Generic));
        size_t stmt_count_offset = unit->bytecode.size();
        emitU32(0);

        CompilerEnvironment block_env(env->context, env, false);
        CompilerEnvironment* saved_env = env;
        cleanup_scopes.push_back(CleanupScope{});
        break_targets.push_back(cleanup_scopes.size() - 1);
        env = &block_env;
        try {
            for (const auto& stmt : expr.statements) {
                stmt->accept(*this);
            }
            emitCleanupScope(cleanup_scopes.back());
            uint32_t block_len = static_cast<uint32_t>(unit->bytecode.size() - (stmt_count_offset + 4));
            for (int i = 0; i < 4; ++i) {
                unit->bytecode[stmt_count_offset + i] = static_cast<uint8_t>((block_len >> (i * 8)) & 0xFF);
            }
            env = saved_env;
        } catch (...) {
            env = saved_env;
            break_targets.pop_back();
            cleanup_scopes.pop_back();
            throw;
        }
        break_targets.pop_back();
        cleanup_scopes.pop_back();
    }

    void visit(const frontend::StructExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeStructDef, Domain::Generic));
        emitU32(static_cast<uint32_t>(expr.fields.size()));
        for (const auto& field : expr.fields) {
            emitStringIndex(std::string(field.name.lexeme));
            if (field.type_bound) {
                unit->emit(1);
                emitU32(unit->addChildUnit(compileExpressionUnit(*field.type_bound)));
            } else {
                unit->emit(0);
            }
            if (field.initializer) {
                unit->emit(1);
                emitU32(unit->addChildUnit(compileExpressionUnit(*field.initializer)));
            } else {
                unit->emit(0);
            }
        }
    }

    void visit(const frontend::IndexExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::Index, Domain::Generic));
        emitOperand(*expr.target);
        if (expr.args.size() != 1) {
            throw std::runtime_error("IndexExpr expects exactly one argument");
        }
        emitOperand(*expr.args[0].value);
    }

    void visit(const frontend::ListExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeArray, Domain::Generic));
        emitU32(static_cast<uint32_t>(expr.elements.size()));
        for (const auto& element : expr.elements) {
            emitOperand(*element);
        }
    }

    void visit(const frontend::MatchExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::Match, Domain::Generic));
        
        size_t total_match_len_offset = unit->bytecode.size();
        emitU32(0); // Placeholder for total match length
        
        size_t match_start = unit->bytecode.size();

        unit->emit(static_cast<uint8_t>(expr.arms.size()));
        emitOperand(*expr.subject);

        for (const auto& arm : expr.arms) {
            size_t arm_header_offset = unit->bytecode.size();
            emitU32(0); // Placeholder for [Result Length (24 bits) | Cond Operand Header (8 bits)]

            size_t cond_start = unit->bytecode.size();
            emitOperand(*arm.pattern);
            
            uint8_t cond_header = unit->bytecode[cond_start];
            unit->bytecode.erase(unit->bytecode.begin() + cond_start);

            size_t result_start = unit->bytecode.size();
            emitOperand(*arm.body);
            
            uint32_t result_len = static_cast<uint32_t>(unit->bytecode.size() - result_start);
            if (result_len > 0xFFFFFF) {
                throw std::runtime_error("Match arm result is too large (exceeds 24 bits)");
            }

            uint32_t packed_header = (static_cast<uint32_t>(cond_header) << 24) | result_len;
            for (int i = 0; i < 4; ++i) {
                unit->bytecode[arm_header_offset + i] = static_cast<uint8_t>((packed_header >> (i * 8)) & 0xFF);
            }
        }
        
        uint32_t total_match_len = static_cast<uint32_t>(unit->bytecode.size() - match_start);
        for (int i = 0; i < 4; ++i) {
            unit->bytecode[total_match_len_offset + i] = static_cast<uint8_t>((total_match_len >> (i * 8)) & 0xFF);
        }
    }



    void visit(const frontend::EnumExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeEnumFamily, Domain::Generic));
        emitU64(expr.node_id);
        unit->emit(static_cast<uint8_t>(expr.variants.size()));
        for (const auto& variant : expr.variants) {
            emitU32(unit->addStringConstant(variant));
        }
    }

    void visit(const frontend::FStringExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeFString, Domain::Generic));
        emitU32(static_cast<uint32_t>(expr.parts.size()));
        for (const auto& part : expr.parts) {
            emitOperand(*part);
        }
    }

    void visit(const frontend::AnonymousStructLiteralExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::MakeAnonStruct, Domain::Generic));
        emitU32(static_cast<uint32_t>(expr.fields.size()));
        for (const auto& field : expr.fields) {
            if (!field.name.has_value()) {
                throw std::runtime_error("Anonymous struct literals require named fields");
            }
            emitStringIndex(std::string(field.name->lexeme));
            emitOperand(*field.value);
        }
    }
};

} // namespace

std::shared_ptr<ProgramUnit> Compiler::compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, bool is_module) {
    auto unit = std::make_shared<ProgramUnit>();
    CompilerContext root_context(nullptr, true);
    CompilerEnvironment root_env(&root_context, nullptr, true);
    CompilerVisitor visitor(unit, &root_env, global_final_bindings_, global_purity_);
    visitor.compileStatements(stmts);
    if (is_module) {
        unit->emit(encodeInstruction(Opcode::MakeAnonStruct, Domain::Generic));
        uint32_t size = static_cast<uint32_t>(visitor.public_globals.size());
        for (int i = 0; i < 4; ++i) {
            unit->emit((size >> (i * 8)) & 0xFF);
        }
        for (const auto& name : visitor.public_globals) {
            uint32_t name_idx = unit->addStringConstant(name);
            for (int i = 0; i < 4; ++i) {
                unit->emit((name_idx >> (i * 8)) & 0xFF);
            }
            unit->emit(static_cast<uint8_t>(OperandType::Identifier));
            for (int i = 0; i < 4; ++i) {
                unit->emit((name_idx >> (i * 8)) & 0xFF);
            }
        }
    }
    unit->num_locals = root_context.next_local;
    return unit;
}

} // namespace chirp::vm
