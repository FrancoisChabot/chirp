#include "compiler.h"

#include "opcodes.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

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
    std::unordered_map<std::string, uint32_t> locals;

    CompilerEnvironment(CompilerContext* current_context, CompilerEnvironment* parent_env = nullptr, bool top_level = false)
        : parent(parent_env), context(current_context), top_level_scope(top_level) {}

    VariableRef resolve(const std::string& name) {
        if (auto local = locals.find(name); local != locals.end()) {
            return {VariableRef::Kind::Local, local->second};
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

class CompilerVisitor : public frontend::ASTVisitor, public frontend::StmtVisitor {
public:
    std::shared_ptr<ProgramUnit> unit;
    CompilerEnvironment* env;

    CompilerVisitor(std::shared_ptr<ProgramUnit> current_unit, CompilerEnvironment* current_env)
        : unit(std::move(current_unit)), env(current_env) {}

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

    std::shared_ptr<ProgramUnit> compileExpressionUnit(const frontend::Expr& expr) {
        auto expr_unit = std::make_shared<ProgramUnit>();
        CompilerContext expr_context(env->context);
        CompilerEnvironment expr_env(&expr_context, env, false);
        CompilerVisitor expr_visitor(expr_unit, &expr_env);
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
        predicate_unit->parameter_names.push_back(std::string(binding.name.lexeme));
        predicate_env.locals.emplace(std::string(binding.name.lexeme), predicate_context.allocateLocal());
        CompilerVisitor predicate_visitor(predicate_unit, &predicate_env);
        predicate_unit->emit(encodeInstruction(Opcode::Return, Domain::Generic));
        predicate_visitor.emitOperand(expr);
        predicate_unit->num_locals = predicate_context.next_local;
        predicate_unit->captures = predicate_context.captures;
        return predicate_unit;
    }

    void emitResolvedVariableOperand(const std::string& name) {
        VariableRef ref = env->resolve(name);
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
    }

    void emitOperand(const frontend::Expr& expr) {
        if (auto grouping = dynamic_cast<const frontend::GroupingExpr*>(&expr)) {
            emitOperand(*grouping->expression);
        } else if (auto num = dynamic_cast<const frontend::NumberExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmInt));
            emitU64(std::stoull(std::string(num->value)));
        } else if (auto ident = dynamic_cast<const frontend::IdentifierExpr*>(&expr)) {
            emitResolvedVariableOperand(std::string(ident->name));
        } else if (auto intrinsic = dynamic_cast<const frontend::IntrinsicExpr*>(&expr)) {
            emitResolvedVariableOperand(std::string(intrinsic->name));
        } else if (auto str = dynamic_cast<const frontend::StringExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmString));
            emitStringIndex(std::string(str->value));
        } else if (auto chr = dynamic_cast<const frontend::CharExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmChar));
            uint32_t value = chr->value.length() >= 3 ? static_cast<unsigned char>(chr->value[1]) : 0;
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

        unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));

        if (env->top_level_scope) {
            unit->emit(static_cast<uint8_t>(OperandType::Identifier));
            emitStringIndex(std::string(stmt.binding.name.lexeme));
        } else {
            uint32_t slot = env->context->allocateLocal();
            unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
            emitU32(slot);
            env->locals.emplace(std::string(stmt.binding.name.lexeme), slot);
        }

        emitOperand(*stmt.binding.initializer);
    }

    void visit(const frontend::BreakStmt& stmt) override {
        unit->emit(encodeInstruction(Opcode::Break, Domain::Generic));
        if (stmt.value) {
            emitOperand(*stmt.value);
        } else {
            unit->emit(static_cast<uint8_t>(OperandType::ImmNull));
        }
    }

    void visit(const frontend::AssignStmt& stmt) override {
        auto* ident = dynamic_cast<const frontend::IdentifierExpr*>(stmt.target.get());
        if (ident == nullptr) {
            throw std::runtime_error("AssignStmt only supports identifier targets in the VM");
        }

        unit->emit(encodeInstruction(Opcode::Assign, Domain::Generic));
        VariableRef ref = env->resolve(std::string(ident->name));
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
                emitStringIndex(std::string(ident->name));
                break;
        }
        emitOperand(*stmt.value);
    }

    void visit(const frontend::IfStmt& stmt) override {
        throw std::runtime_error("IfStmt is not supported in the VM yet");
    }

    void visit(const frontend::DebugStmt& stmt) override {
        throw std::runtime_error("DebugStmt is not supported in the VM yet");
    }

    void visit(const frontend::BinaryExpr& expr) override {
        switch (expr.op) {
            case frontend::BinaryOp::Add:
            case frontend::BinaryOp::Sub:
            case frontend::BinaryOp::Mul:
            case frontend::BinaryOp::Div:
            case frontend::BinaryOp::Mod:
                unit->emit(encodeInstruction(Opcode::BinaryMath, Domain::Generic));
                switch (expr.op) {
                    case frontend::BinaryOp::Add: unit->emit(static_cast<uint8_t>(BinaryMathOp::Add)); break;
                    case frontend::BinaryOp::Sub: unit->emit(static_cast<uint8_t>(BinaryMathOp::Sub)); break;
                    case frontend::BinaryOp::Mul: unit->emit(static_cast<uint8_t>(BinaryMathOp::Mul)); break;
                    case frontend::BinaryOp::Div: unit->emit(static_cast<uint8_t>(BinaryMathOp::Div)); break;
                    case frontend::BinaryOp::Mod: unit->emit(static_cast<uint8_t>(BinaryMathOp::Mod)); break;
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
                throw std::runtime_error("Unsupported binary op in the VM");
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
        CompilerContext lambda_context(env->context);
        CompilerEnvironment lambda_env(&lambda_context, env, false);

        for (const auto& param : expr.parameters) {
            lambda_unit->parameter_names.push_back(std::string(param.name.lexeme));
            lambda_env.locals.emplace(std::string(param.name.lexeme), lambda_context.allocateLocal());
        }

        CompilerVisitor lambda_visitor(lambda_unit, &lambda_env);
        lambda_unit->emit(encodeInstruction(Opcode::Return, Domain::Generic));
        lambda_visitor.emitOperand(*expr.body);
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
        if (expr.op == frontend::UnaryOp::Not) {
            unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
            emitOperand(*expr.right);

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
            return;
        }
        throw std::runtime_error("UnaryExpr is not supported in the VM yet");
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
        unit->emit(encodeInstruction(Opcode::MakeEnumSet, Domain::Generic));
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
        throw std::runtime_error("WhileExpr is not supported in the VM yet");
    }

    void visit(const frontend::ForExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::ForEach, Domain::Generic));
        emitOperand(*expr.iterable);
        uint32_t slot = env->context->allocateLocal();
        emitU32(slot);

        CompilerEnvironment loop_env(env->context, env, false);
        loop_env.locals.emplace(std::string(expr.iterator_binding.name.lexeme), slot);

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
            throw;
        }
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
        emitU32(static_cast<uint32_t>(expr.statements.size()));

        CompilerEnvironment block_env(env->context, env, false);
        CompilerEnvironment* saved_env = env;
        env = &block_env;
        try {
            for (const auto& stmt : expr.statements) {
                stmt->accept(*this);
            }
            env = saved_env;
        } catch (...) {
            env = saved_env;
            throw;
        }
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
        throw std::runtime_error("ListExpr is not supported in the VM yet");
    }

    void visit(const frontend::MatchExpr& expr) override {
        throw std::runtime_error("MatchExpr is not supported in the VM yet");
    }

    void visit(const frontend::EnumExpr& expr) override {
        throw std::runtime_error("EnumExpr is not supported in the VM yet");
    }

    void visit(const frontend::FStringExpr& expr) override {
        throw std::runtime_error("FStringExpr is not supported in the VM yet");
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

std::shared_ptr<ProgramUnit> Compiler::compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
    auto unit = std::make_shared<ProgramUnit>();
    CompilerContext root_context(nullptr, true);
    CompilerEnvironment root_env(&root_context, nullptr, true);
    CompilerVisitor visitor(unit, &root_env);
    visitor.compileStatements(stmts);
    unit->num_locals = root_context.next_local;
    return unit;
}

} // namespace chirp::vm
