#include "evaluator.h"
#include "opcodes.h"
#include <stdexcept>

namespace chirp::vm {

class ExecutionState {
public:
    const ProgramUnit& unit;
    size_t pc = 0;
    std::vector<Value> locals;

    ExecutionState(const ProgramUnit& u) : unit(u), locals(u.num_locals) {}

    uint8_t read8() {
        if (pc >= unit.bytecode.size()) {
            throw std::runtime_error("Unexpected end of bytecode");
        }
        return unit.bytecode[pc++];
    }

    uint64_t read64() {
        uint64_t val = 0;
        for (int i = 0; i < 8; ++i) {
            val |= (static_cast<uint64_t>(read8()) << (i * 8));
        }
        return val;
    }

    Value evalOperand() {
        OperandType type = static_cast<OperandType>(read8());
        switch (type) {
            case OperandType::ImmU64:
                return Value(read64());
            case OperandType::Inline:
                return evalInstruction();
            default:
                throw std::runtime_error("Unsupported operand type in evalOperand");
        }
    }

    Value evalInstruction() {
        uint8_t inst = read8();
        Opcode op = decodeOpcode(inst);
        Domain dom = decodeDomain(inst);

        if (op == Opcode::BinaryMath) {
            BinaryMathOp math_op = static_cast<BinaryMathOp>(read8());
            Value left = evalOperand();
            Value right = evalOperand();

            if (dom == Domain::Generic) {
                switch (math_op) {
                    case BinaryMathOp::Add: return Value(left.as_int + right.as_int);
                    case BinaryMathOp::Sub: return Value(left.as_int - right.as_int);
                    case BinaryMathOp::Mul: return Value(left.as_int * right.as_int);
                    case BinaryMathOp::Div: 
                        if (right.as_int == 0) throw std::runtime_error("Division by zero");
                        return Value(left.as_int / right.as_int);
                    case BinaryMathOp::Mod:
                        if (right.as_int == 0) throw std::runtime_error("Modulo by zero");
                        return Value(left.as_int % right.as_int);
                    default:
                        throw std::runtime_error("Unknown BinaryMathOp");
                }
            } else {
                throw std::runtime_error("Specialized domains not implemented");
            }
        }

        throw std::runtime_error("Unsupported instruction in basic evaluator");
    }
    
    Value run() {
        Value last(0);
        while (pc < unit.bytecode.size()) {
            last = evalInstruction();
        }
        return last;
    }
};

Value Evaluator::evaluate(const ProgramUnit& unit) {
    ExecutionState state(unit);
    return state.run();
}

} // namespace chirp::vm
