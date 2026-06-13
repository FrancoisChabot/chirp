#include "evaluator.h"
#include "opcodes.h"
#include <stdexcept>
#include <iostream>

namespace chirp::vm {

class ExecutionState {
public:
    std::shared_ptr<ProgramUnit> unit;
    size_t pc = 0;
    std::vector<Value> locals;
    std::unordered_map<std::string, Value>& globals;
    std::ostream& out;

    ExecutionState(std::shared_ptr<ProgramUnit> u, std::unordered_map<std::string, Value>& g, std::ostream& o) 
        : unit(u), locals(u->num_locals), globals(g), out(o) {}

    uint8_t read8() {
        if (pc >= unit->bytecode.size()) {
            throw std::runtime_error("Unexpected end of bytecode");
        }
        return unit->bytecode[pc++];
    }

    uint16_t read16() {
        uint16_t val = read8();
        val |= (static_cast<uint16_t>(read8()) << 8);
        return val;
    }

    uint32_t read32() {
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            val |= (static_cast<uint32_t>(read8()) << (i * 8));
        }
        return val;
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
            case OperandType::ImmInt:
                return Value(static_cast<int64_t>(read64()));
            case OperandType::ImmString: {
                uint32_t idx = read32();
                std::string str = unit->constant_strings.at(idx);
                return Value(str);
            }
            case OperandType::ImmChar: {
                uint32_t val = read32();
                return Value::Char(val);
            }
            case OperandType::ImmSymbol: {
                uint32_t idx = read32();
                std::string str = unit->constant_strings.at(idx);
                return Value::Symbol(str);
            }
            case OperandType::Inline:
                return evalInstruction();
            case OperandType::StackLocal:
                return locals.at(read8());
            case OperandType::Identifier: {
                uint32_t idx = read32();
                std::string name = unit->constant_strings.at(idx);
                if (globals.contains(name)) {
                    return globals[name];
                }
                throw std::runtime_error("Undefined global variable: " + name);
            }
            default:
                throw std::runtime_error("Unsupported operand type in evalOperand");
        }
    }

    Value evalInstruction() {
        uint8_t inst = read8();
        Opcode op = decodeOpcode(inst);
        Domain dom = decodeDomain(inst);

        switch (op) {
            case Opcode::GetField: {
                Value target = evalOperand();
                Value field = evalOperand();
                if (target.type != ValueType::Struct) {
                    throw std::runtime_error("GetField target must be a struct");
                }
                auto it = target.as_struct->find(field.as_string);
                if (it == target.as_struct->end()) {
                    throw std::runtime_error("Struct missing field: " + field.as_string);
                }
                return it->second;
            }
            case Opcode::Index: {
                Value target = evalOperand();
                Value index = evalOperand();
                if (target.type == ValueType::Array) {
                    if (index.type != ValueType::Int) {
                        throw std::runtime_error("Array index must be an integer");
                    }
                    if (index.as_int < 0 || index.as_int >= target.as_array->size()) {
                        throw std::runtime_error("Array index out of bounds");
                    }
                    return target.as_array->at(index.as_int);
                }
                throw std::runtime_error("Index operation on non-array type");
            }
            case Opcode::BinaryMath: {
                BinaryMathOp math_op = static_cast<BinaryMathOp>(read8());
                Value left = evalOperand();
                Value right = evalOperand();

                if (dom == Domain::Generic) {
                    if (left.type != ValueType::Int || right.type != ValueType::Int) {
                        throw std::runtime_error("Type error: expected integers for math");
                    }
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
            case Opcode::Compare: {
                CompareOp cmp_op = static_cast<CompareOp>(read8());
                Value left = evalOperand();
                Value right = evalOperand();

                if (dom == Domain::Generic) {
                    if (left.type != ValueType::Int || right.type != ValueType::Int) {
                        throw std::runtime_error("Type error: expected integers for comparison");
                    }
                    switch (cmp_op) {
                        case CompareOp::Eq: return Value(static_cast<bool>(left.as_int == right.as_int));
                        case CompareOp::Neq: return Value(static_cast<bool>(left.as_int != right.as_int));
                        case CompareOp::Lt: return Value(static_cast<bool>(left.as_int < right.as_int));
                        case CompareOp::Lte: return Value(static_cast<bool>(left.as_int <= right.as_int));
                        case CompareOp::Gt: return Value(static_cast<bool>(left.as_int > right.as_int));
                        case CompareOp::Gte: return Value(static_cast<bool>(left.as_int >= right.as_int));
                        default: throw std::runtime_error("Unknown CompareOp");
                    }
                } else {
                    throw std::runtime_error("Specialized domains not implemented");
                }
            }
            case Opcode::If: {
                Value cond = evalOperand();
                uint16_t true_len = read16();
                
                bool is_true = (cond.type == ValueType::Int && cond.as_int != 0);

                if (is_true) {
                    Value res = evalOperand(); // evaluate true branch
                    uint16_t false_len = read16(); // read false_len
                    pc += false_len; // skip false branch
                    return res;
                } else {
                    pc += true_len; // skip true branch
                    uint16_t false_len = read16(); // read false_len
                    return evalOperand(); // evaluate false branch
                }
            }
            case Opcode::MakeLambda: {
                uint32_t idx = read32();
                return Value(std::make_shared<Closure>(Closure{unit->child_units.at(idx)}));
            }
            case Opcode::Call: {
                Value callee = evalOperand();
                uint8_t num_args = read8();
                std::vector<Value> args(num_args);
                for (int i = 0; i < num_args; ++i) {
                    args[i] = evalOperand();
                }

                if (callee.type == ValueType::NativeFunc) {
                    return (*callee.as_native)(args);
                }

                if (callee.type != ValueType::Closure) {
                    throw std::runtime_error("Type error: target is not callable");
                }

                ExecutionState call_state(callee.as_closure->unit, globals, out);
                // For simplicity, we just pass args into the first N locals.
                for (size_t i = 0; i < args.size() && i < call_state.locals.size(); ++i) {
                    call_state.locals[i] = args[i];
                }
                return call_state.run();
            }
            case Opcode::Block: {
                uint8_t num_stmts = read8();
                Value last;
                for (int i = 0; i < num_stmts; ++i) {
                    last = evalInstruction();
                }
                return last;
            }
            case Opcode::Let: {
                OperandType ident_type = static_cast<OperandType>(read8());
                if (ident_type != OperandType::Identifier) {
                    throw std::runtime_error("Let instruction requires Identifier operand");
                }
                uint32_t idx = read32();
                std::string name = unit->constant_strings.at(idx);
                Value val = evalOperand();
                globals[name] = val;
                return val;
            }
            default:
                throw std::runtime_error("Unsupported instruction: " + std::to_string(static_cast<int>(op)));
        }
    }
    
    Value run() {
        Value last;
        while (pc < unit->bytecode.size()) {
            last = evalInstruction();
        }
        return last;
    }
};

Value Evaluator::evaluate(std::shared_ptr<ProgramUnit> unit, std::unordered_map<std::string, Value>& globals, std::ostream& out) {
    ExecutionState state(unit, globals, out);
    return state.run();
}

} // namespace chirp::vm
