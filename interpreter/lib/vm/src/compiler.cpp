#include "compiler.h"
#include "opcodes.h"
#include <stdexcept>
#include <string>

namespace chirp::vm {

class CompilerVisitor : public frontend::ASTVisitor, public frontend::StmtVisitor {
public:
    ProgramUnit unit;

    void compile_statements(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
        // Emit a Block instruction for the root sequence?
        // Actually, just evaluate them sequentially and return the last value.
        // For simple math, we only support ExprStmt.
        for (const auto& stmt : stmts) {
            stmt->accept(*this);
        }
    }

    // StmtVisitor
    void visit(const frontend::ExprStmt& stmt) override {
        stmt.expression->accept(*this);
        // We evaluate expressions, but what do we do with the result? 
        // For the REPL, the session could just print the final result.
    }
    
    void visit(const frontend::LetStmt& stmt) override { throw std::runtime_error("LetStmt not supported yet"); }
    void visit(const frontend::BreakStmt& stmt) override { throw std::runtime_error("BreakStmt not supported yet"); }
    void visit(const frontend::AssignStmt& stmt) override { throw std::runtime_error("AssignStmt not supported yet"); }
    void visit(const frontend::IfStmt& stmt) override { throw std::runtime_error("IfStmt not supported yet"); }
    void visit(const frontend::DebugStmt& stmt) override { throw std::runtime_error("DebugStmt not supported yet"); }

    // ASTVisitor
    void visit(const frontend::BinaryExpr& expr) override {
        unit.emit(encodeInstruction(Opcode::BinaryMath, Domain::Generic));
        
        switch (expr.op) {
            case frontend::BinaryOp::Add: unit.emit(static_cast<uint8_t>(BinaryMathOp::Add)); break;
            case frontend::BinaryOp::Sub: unit.emit(static_cast<uint8_t>(BinaryMathOp::Sub)); break;
            case frontend::BinaryOp::Mul: unit.emit(static_cast<uint8_t>(BinaryMathOp::Mul)); break;
            case frontend::BinaryOp::Div: unit.emit(static_cast<uint8_t>(BinaryMathOp::Div)); break;
            case frontend::BinaryOp::Mod: unit.emit(static_cast<uint8_t>(BinaryMathOp::Mod)); break;
            default: throw std::runtime_error("Unsupported binary op in simple math mode");
        }

        // Operands
        emitOperand(*expr.left);
        emitOperand(*expr.right);
    }

    void visit(const frontend::NumberExpr& expr) override {
        // We emit it as a naked instruction so it can be picked up by the inline operand evaluation.
        // But wait! If an inline expression expects an *instruction*, and we are just an immediate...
        // Actually, there is no "Immediate Instruction", there's only an "Immediate Operand".
        // If the compiler emits `OperandType::Inline` then it MUST emit a valid instruction next.
        // Wait, if an operand type is `OperandType::ImmU8`, it wouldn't need an instruction opcode.
        // BUT `BinaryExpr` above emits `OperandType::Inline` for the children! 
        // So the children must emit INSTRUCTIONS. 
        // Is there a "LoadImmediate" instruction?
        // We didn't define one. We defined `Let` and `Assign` which take operands.
        // Wait, if `BinaryMath` takes operands, and we evaluate `1 + 2`, we should NOT emit `Inline` for `1` and `2`.
        // We should peek at the child and see if it's a NumberExpr, and if so emit `ImmU8`.
        // Or we add a `Return` instruction that takes an operand and returns it, acting as an identity.
        // Let's just do the peek optimization for numbers for now, or add a `Return` instruction.
        throw std::runtime_error("Naked NumberExpr encountered where an instruction was expected.");
    }
    
    // We need a helper to emit operands
    void emitOperand(const frontend::Expr& expr) {
        if (auto num = dynamic_cast<const frontend::NumberExpr*>(&expr)) {
            // For simplicity, we just use ImmU64 for all numbers right now.
            unit.emit(static_cast<uint8_t>(OperandType::ImmU64));
            uint64_t val = std::stoull(std::string(num->value));
            for (int i = 0; i < 8; ++i) {
                unit.emit((val >> (i * 8)) & 0xFF);
            }
        } else {
            unit.emit(static_cast<uint8_t>(OperandType::Inline));
            expr.accept(*this);
        }
    }

    void visit(const frontend::UnaryExpr& expr) override { throw std::runtime_error("Unsupported in simple math mode"); }
    void visit(const frontend::GroupingExpr& expr) override { expr.expression->accept(*this); }
    void visit(const frontend::StringExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::CharExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::IdentifierExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::IntrinsicExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::SymbolicConstantExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::EnumeratedSetExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::ConstructedSetExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::AnonymousStructLiteralExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::IfExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::WhileExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::ForExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::LambdaExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::SignatureExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::BlockExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::StructExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::CallExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::IndexExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::ListExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::MatchExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::EnumExpr& expr) override { throw std::runtime_error("Unsupported"); }
    void visit(const frontend::FStringExpr& expr) override { throw std::runtime_error("Unsupported"); }
};

ProgramUnit Compiler::compile(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) {
    CompilerVisitor visitor;
    visitor.compile_statements(stmts);
    return visitor.unit;
}

} // namespace chirp::vm
