#include "compiler.h"
#include "opcodes.h"
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace chirp::vm {

class CompilerEnvironment {
public:
    CompilerEnvironment* parent = nullptr;
    std::unordered_map<std::string, uint32_t> locals;
    uint32_t current_locals = 0;

    CompilerEnvironment(CompilerEnvironment* p = nullptr) : parent(p) {}

    void defineLocal(const std::string& name) {
        locals[name] = current_locals++;
    }

    std::optional<uint32_t> resolveLocal(const std::string& name) {
        if (locals.contains(name)) return locals[name];
        return std::nullopt;
    }
};

class CompilerVisitor : public frontend::ASTVisitor, public frontend::StmtVisitor {
public:
    std::shared_ptr<ProgramUnit> unit;
    CompilerEnvironment* env;

    CompilerVisitor(std::shared_ptr<ProgramUnit> u, CompilerEnvironment* e) : unit(u), env(e) {}

    void compile_statements(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
        for (const auto& stmt : stmts) {
            stmt->accept(*this);
        }
    }

    void emitOperand(const frontend::Expr& expr) {
        if (auto num = dynamic_cast<const frontend::NumberExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmInt));
            uint64_t val = std::stoull(std::string(num->value));
            for (int i = 0; i < 8; ++i) {
                unit->emit((val >> (i * 8)) & 0xFF);
            }
        } else if (auto ident = dynamic_cast<const frontend::IdentifierExpr*>(&expr)) {
            auto local = env->resolveLocal(std::string(ident->name));
            if (local.has_value()) {
                unit->emit(static_cast<uint8_t>(OperandType::StackLocal));
                unit->emit(static_cast<uint8_t>(*local)); // Assume < 256 locals for now
            } else {
                unit->emit(static_cast<uint8_t>(OperandType::Identifier));
                uint32_t idx = unit->addStringConstant(std::string(ident->name));
                for (int i = 0; i < 4; ++i) {
                    unit->emit((idx >> (i * 8)) & 0xFF);
                }
            }
        } else if (auto intrinsic = dynamic_cast<const frontend::IntrinsicExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::Identifier));
            uint32_t idx = unit->addStringConstant(std::string(intrinsic->name));
            for (int i = 0; i < 4; ++i) {
                unit->emit((idx >> (i * 8)) & 0xFF);
            }
        } else if (auto str = dynamic_cast<const frontend::StringExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmString));
            uint32_t idx = unit->addStringConstant(std::string(str->value));
            for (int i = 0; i < 4; ++i) {
                unit->emit((idx >> (i * 8)) & 0xFF);
            }
        } else if (auto chr = dynamic_cast<const frontend::CharExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmChar));
            uint32_t val = chr->value.length() >= 3 ? chr->value[1] : 0; // simple unescape
            for (int i = 0; i < 4; ++i) {
                unit->emit((val >> (i * 8)) & 0xFF);
            }
        } else if (auto sym = dynamic_cast<const frontend::SymbolicConstantExpr*>(&expr)) {
            unit->emit(static_cast<uint8_t>(OperandType::ImmSymbol));
            uint32_t idx = unit->addStringConstant(std::string(sym->value));
            for (int i = 0; i < 4; ++i) {
                unit->emit((idx >> (i * 8)) & 0xFF);
            }
        } else {
            unit->emit(static_cast<uint8_t>(OperandType::Inline));
            expr.accept(*this);
        }
    }

    void emitU16(uint16_t val) {
        unit->emit(val & 0xFF);
        unit->emit((val >> 8) & 0xFF);
    }

    // StmtVisitor
    void visit(const frontend::ExprStmt& stmt) override {
        stmt.expression->accept(*this);
    }
    
    void visit(const frontend::LetStmt& stmt) override {
        // Evaluate the initializer
        unit->emit(encodeInstruction(Opcode::Let, Domain::Generic));
        
        // Operand 1: The identifier name as a constant pool index
        unit->emit(static_cast<uint8_t>(OperandType::Identifier));
        uint32_t idx = unit->addStringConstant(std::string(stmt.binding.name.lexeme));
        for (int i = 0; i < 4; ++i) {
            unit->emit((idx >> (i * 8)) & 0xFF);
        }

        // Operand 2: Initializer
        if (stmt.binding.initializer) {
            emitOperand(*stmt.binding.initializer);
        } else {
            throw std::runtime_error("LetStmt without initializer not supported yet in simple VM");
        }
    }

    void visit(const frontend::BreakStmt& stmt) override { throw std::runtime_error("Unsupported BreakStmt"); }
    void visit(const frontend::AssignStmt& stmt) override { throw std::runtime_error("Unsupported AssignStmt"); }
    void visit(const frontend::IfStmt& stmt) override { throw std::runtime_error("Unsupported IfStmt"); }
    void visit(const frontend::DebugStmt& stmt) override { throw std::runtime_error("Unsupported DebugStmt"); }

    // ASTVisitor
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
                break;
            case frontend::BinaryOp::Dot: {
                unit->emit(encodeInstruction(Opcode::GetField, Domain::Generic));
                emitOperand(*expr.left);
                if (auto ident = dynamic_cast<const frontend::IdentifierExpr*>(expr.right.get())) {
                    unit->emit(static_cast<uint8_t>(OperandType::ImmString));
                    uint32_t idx = unit->addStringConstant(std::string(ident->name));
                    for (int i = 0; i < 4; ++i) {
                        unit->emit((idx >> (i * 8)) & 0xFF);
                    }
                } else {
                    throw std::runtime_error("Dot right hand side must be an identifier");
                }
                break;
            }
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
                break;
            default:
                throw std::runtime_error("Unsupported binary op in simple math mode");
        }
    }

    void visit(const frontend::IfExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::If, Domain::Generic));
        emitOperand(*expr.condition);

        size_t true_len_offset = unit->bytecode.size();
        emitU16(0); // placeholder
        size_t true_start = unit->bytecode.size();
        emitOperand(*expr.then_branch);
        size_t true_len = unit->bytecode.size() - true_start;
        unit->bytecode[true_len_offset] = true_len & 0xFF;
        unit->bytecode[true_len_offset + 1] = (true_len >> 8) & 0xFF;

        size_t false_len_offset = unit->bytecode.size();
        emitU16(0); // placeholder
        size_t false_start = unit->bytecode.size();
        emitOperand(*expr.else_branch);
        size_t false_len = unit->bytecode.size() - false_start;
        unit->bytecode[false_len_offset] = false_len & 0xFF;
        unit->bytecode[false_len_offset + 1] = (false_len >> 8) & 0xFF;
    }

    void visit(const frontend::LambdaExpr& expr) override {
        auto lambda_unit = std::make_shared<ProgramUnit>();
        CompilerEnvironment lambda_env(env);

        for (const auto& param : expr.parameters) {
            lambda_env.defineLocal(std::string(param.name.lexeme));
        }

        CompilerVisitor lambda_visitor(lambda_unit, &lambda_env);
        expr.body->accept(lambda_visitor);
        lambda_unit->num_locals = lambda_env.current_locals;

        unit->emit(encodeInstruction(Opcode::MakeLambda, Domain::Generic));
        uint32_t idx = unit->addChildUnit(lambda_unit);
        for (int i = 0; i < 4; ++i) {
            unit->emit((idx >> (i * 8)) & 0xFF);
        }
    }

    void visit(const frontend::CallExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::Call, Domain::Generic));
        emitOperand(*expr.callee);
        
        unit->emit(static_cast<uint8_t>(expr.args.size()));
        for (const auto& arg : expr.args) {
            emitOperand(*arg.value);
        }
    }

    void visit(const frontend::IdentifierExpr& expr) override {
        // Handled via emitOperand typically, but if visited directly:
        auto local = env->resolveLocal(std::string(expr.name));
        if (local.has_value()) {
            throw std::runtime_error("Identifier visited directly should not be a local in simple VM");
        } else {
            // It's a global lookup without operand wrapping, wait, this should never happen since we use emitOperand for everything.
            throw std::runtime_error("Identifier visited directly");
        }
    }
    
    void visit(const frontend::NumberExpr& expr) override {
        throw std::runtime_error("Naked NumberExpr encountered where an instruction was expected.");
    }
    
    void visit(const frontend::UnaryExpr& expr) override { throw std::runtime_error("Unsupported UnaryExpr"); }
    void visit(const frontend::GroupingExpr& expr) override { expr.expression->accept(*this); }
    void visit(const frontend::StringExpr& expr) override { throw std::runtime_error("Cannot use StringExpr as statement"); }
    void visit(const frontend::CharExpr& expr) override { throw std::runtime_error("Cannot use CharExpr as statement"); }
    void visit(const frontend::IntrinsicExpr& expr) override { throw std::runtime_error("Cannot use IntrinsicExpr as statement"); }
    void visit(const frontend::SymbolicConstantExpr& expr) override { throw std::runtime_error("Cannot use SymbolicConstantExpr as statement"); }
    void visit(const frontend::EnumeratedSetExpr& expr) override { throw std::runtime_error("Unsupported: EnumeratedSetExpr"); }
    void visit(const frontend::ConstructedSetExpr& expr) override { throw std::runtime_error("Unsupported: ConstructedSetExpr"); }
    void visit(const frontend::AnonymousStructLiteralExpr& expr) override { throw std::runtime_error("Unsupported: AnonymousStructLiteralExpr"); }
    void visit(const frontend::WhileExpr& expr) override { throw std::runtime_error("Unsupported: WhileExpr"); }
    void visit(const frontend::ForExpr& expr) override { throw std::runtime_error("Unsupported: ForExpr"); }
    void visit(const frontend::SignatureExpr& expr) override { throw std::runtime_error("Unsupported: SignatureExpr"); }
    void visit(const frontend::BlockExpr& expr) override { 
        unit->emit(encodeInstruction(Opcode::Block, Domain::Generic));
        unit->emit(static_cast<uint8_t>(expr.statements.size()));
        for (const auto& stmt : expr.statements) {
            stmt->accept(*this);
        }
    }
    void visit(const frontend::StructExpr& expr) override { throw std::runtime_error("Unsupported: StructExpr"); }
    void visit(const frontend::IndexExpr& expr) override {
        unit->emit(encodeInstruction(Opcode::Index, Domain::Generic));
        emitOperand(*expr.target);
        if (expr.args.size() != 1) throw std::runtime_error("IndexExpr expects exactly 1 argument");
        emitOperand(*expr.args[0].value);
    }
    void visit(const frontend::ListExpr& expr) override { throw std::runtime_error("Unsupported: ListExpr"); }
    void visit(const frontend::MatchExpr& expr) override { throw std::runtime_error("Unsupported: MatchExpr"); }
    void visit(const frontend::EnumExpr& expr) override { throw std::runtime_error("Unsupported: EnumExpr"); }
    void visit(const frontend::FStringExpr& expr) override { throw std::runtime_error("Unsupported: FStringExpr"); }
};

std::shared_ptr<ProgramUnit> Compiler::compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
    auto unit = std::make_shared<ProgramUnit>();
    CompilerEnvironment env;
    CompilerVisitor visitor(unit, &env);
    visitor.compile_statements(stmts);
    unit->num_locals = env.current_locals;
    return unit;
}

} // namespace chirp::vm
