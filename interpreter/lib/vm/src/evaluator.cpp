#include "evaluator.h"

#include "opcodes.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace chirp::vm {

namespace {

struct BreakSignal {
    Value value;
};

class ExecutionState {
public:
    std::shared_ptr<ProgramUnit> unit;
    size_t pc = 0;
    std::vector<Value> locals;
    const std::vector<Value>* captures = nullptr;
    std::unordered_map<std::string, Value>& globals;
    std::ostream& out;

    ExecutionState(std::shared_ptr<ProgramUnit> current_unit,
                   std::unordered_map<std::string, Value>& global_values,
                   std::ostream& output,
                   const std::vector<Value>* captured_values = nullptr)
        : unit(std::move(current_unit)),
          locals(unit->num_locals),
          captures(captured_values),
          globals(global_values),
          out(output) {}

    uint8_t read8() {
        if (pc >= unit->bytecode.size()) {
            throw std::runtime_error("Unexpected end of bytecode");
        }
        return unit->bytecode[pc++];
    }

    uint32_t read32() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= (static_cast<uint32_t>(read8()) << (i * 8));
        }
        return value;
    }

    uint64_t read64() {
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= (static_cast<uint64_t>(read8()) << (i * 8));
        }
        return value;
    }

    Value evalOperand() {
        OperandType type = static_cast<OperandType>(read8());
        switch (type) {
            case OperandType::ImmInt:
                return Value(static_cast<int64_t>(read64()));
            case OperandType::ImmString:
                return Value(unit->constant_strings.at(read32()));
            case OperandType::ImmChar:
                return Value::Char(read32());
            case OperandType::ImmSymbol:
                return Value::Symbol(unit->constant_strings.at(read32()));
            case OperandType::ImmNull:
                return Value();
            case OperandType::Inline:
                return evalInstruction();
            case OperandType::StackLocal:
                return locals.at(read32());
            case OperandType::Capture:
                if (captures == nullptr) {
                    throw std::runtime_error("Capture read with no closure environment");
                }
                return captures->at(read32());
            case OperandType::Identifier: {
                std::string name = unit->constant_strings.at(read32());
                if (globals.contains(name)) {
                    return globals.at(name);
                }
                throw std::runtime_error("Undefined global variable: " + name);
            }
            default:
                throw std::runtime_error("Unsupported operand type in evalOperand");
        }
    }

    std::vector<CallArgument> readCallArguments() {
        uint32_t num_args = read32();
        std::vector<CallArgument> args;
        args.reserve(num_args);
        for (uint32_t i = 0; i < num_args; ++i) {
            bool has_name = read8() != 0;
            std::optional<std::string> name;
            if (has_name) {
                name = unit->constant_strings.at(read32());
            }
            args.push_back(CallArgument{std::move(name), evalOperand()});
        }
        return args;
    }

    static void reject_mixed_call_arguments(const std::vector<CallArgument>& args) {
        bool has_named = false;
        bool has_positional = false;
        for (const auto& arg : args) {
            has_named = has_named || arg.name.has_value();
            has_positional = has_positional || !arg.name.has_value();
        }
        if (has_named && has_positional) {
            throw std::runtime_error("Cannot mix named and positional arguments");
        }
    }

    static std::vector<Value> lower_closure_arguments(const Closure& closure, const std::vector<CallArgument>& args) {
        reject_mixed_call_arguments(args);

        const auto& parameter_names = closure.unit->parameter_names;
        std::vector<Value> lowered(parameter_names.size());

        bool has_named = false;
        for (const auto& arg : args) {
            has_named = has_named || arg.name.has_value();
        }

        if (!has_named) {
            if (args.size() != parameter_names.size()) {
                throw std::runtime_error(
                    "Function expected " + std::to_string(parameter_names.size()) +
                    " arguments, got " + std::to_string(args.size()));
            }
            for (size_t i = 0; i < args.size(); ++i) {
                lowered[i] = args[i].value;
            }
            return lowered;
        }

        std::unordered_map<std::string, size_t> indices;
        for (size_t i = 0; i < parameter_names.size(); ++i) {
            indices.emplace(parameter_names[i], i);
        }

        std::vector<bool> provided(parameter_names.size(), false);
        for (const auto& arg : args) {
            const std::string& name = *arg.name;
            auto found = indices.find(name);
            if (found == indices.end()) {
                throw std::runtime_error("Unknown parameter '" + name + "'");
            }
            size_t index = found->second;
            if (provided[index]) {
                throw std::runtime_error("Duplicate argument for parameter '" + name + "'");
            }
            lowered[index] = arg.value;
            provided[index] = true;
        }

        for (size_t i = 0; i < parameter_names.size(); ++i) {
            if (!provided[i]) {
                throw std::runtime_error("Missing argument for parameter '" + parameter_names[i] + "'");
            }
        }
        return lowered;
    }

    Value evalInstruction() {
        Opcode op = decodeOpcode(read8());
        Domain dom = decodeDomain(unit->bytecode[pc - 1]);

        switch (op) {
            case Opcode::Eval:
                return evalOperand();
            case Opcode::Block: {
                uint32_t num_stmts = read32();
                try {
                    for (uint32_t i = 0; i < num_stmts; ++i) {
                        evalInstruction();
                    }
                    return Value();
                } catch (BreakSignal& signal) {
                    return std::move(signal.value);
                }
            }
            case Opcode::Break:
                throw BreakSignal{evalOperand()};
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
                if (target.type != ValueType::Array) {
                    throw std::runtime_error("Index operation on non-array type");
                }
                if (index.type != ValueType::Int) {
                    throw std::runtime_error("Array index must be an integer");
                }
                if (index.as_int < 0 || static_cast<size_t>(index.as_int) >= target.as_array->size()) {
                    throw std::runtime_error("Array index out of bounds");
                }
                return target.as_array->at(static_cast<size_t>(index.as_int));
            }
            case Opcode::BinaryMath: {
                BinaryMathOp math_op = static_cast<BinaryMathOp>(read8());
                Value left = evalOperand();
                Value right = evalOperand();

                if (dom != Domain::Generic) {
                    throw std::runtime_error("Specialized domains are not implemented");
                }
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
            }
            case Opcode::Compare: {
                CompareOp cmp_op = static_cast<CompareOp>(read8());
                Value left = evalOperand();
                Value right = evalOperand();

                if (dom != Domain::Generic) {
                    throw std::runtime_error("Specialized domains are not implemented");
                }
                if (left.type != ValueType::Int || right.type != ValueType::Int) {
                    throw std::runtime_error("Type error: expected integers for comparison");
                }

                switch (cmp_op) {
                    case CompareOp::Eq: return Value(left.as_int == right.as_int);
                    case CompareOp::Neq: return Value(left.as_int != right.as_int);
                    case CompareOp::Lt: return Value(left.as_int < right.as_int);
                    case CompareOp::Lte: return Value(left.as_int <= right.as_int);
                    case CompareOp::Gt: return Value(left.as_int > right.as_int);
                    case CompareOp::Gte: return Value(left.as_int >= right.as_int);
                    default:
                        throw std::runtime_error("Unknown CompareOp");
                }
            }
            case Opcode::If: {
                Value cond = evalOperand();
                uint32_t true_len = read32();

                bool is_true = false;
                if (cond.type == ValueType::Bool || cond.type == ValueType::Int) {
                    is_true = cond.as_int != 0;
                }

                if (is_true) {
                    Value result = evalOperand();
                    pc += read32();
                    return result;
                }

                pc += true_len;
                [[maybe_unused]] uint32_t false_len = read32();
                return evalOperand();
            }
            case Opcode::MakeLambda: {
                auto child = unit->child_units.at(read32());
                std::vector<Value> captured_values;
                captured_values.reserve(child->captures.size());
                for (const auto& capture : child->captures) {
                    switch (capture.kind) {
                        case CaptureSourceKind::Local:
                            captured_values.push_back(locals.at(capture.index));
                            break;
                        case CaptureSourceKind::Capture:
                            if (captures == nullptr) {
                                throw std::runtime_error("Nested capture read with no closure environment");
                            }
                            captured_values.push_back(captures->at(capture.index));
                            break;
                    }
                }
                return Value(std::make_shared<Closure>(Closure{std::move(child), std::move(captured_values)}));
            }
            case Opcode::Call: {
                Value callee = evalOperand();
                std::vector<CallArgument> args = readCallArguments();

                if (callee.type == ValueType::NativeFunc) {
                    return (*callee.as_native)(args);
                }
                if (callee.type != ValueType::Closure) {
                    throw std::runtime_error("Type error: target is not callable");
                }

                std::vector<Value> lowered_args = lower_closure_arguments(*callee.as_closure, args);
                ExecutionState call_state(callee.as_closure->unit, globals, out, &callee.as_closure->captures);
                for (size_t i = 0; i < lowered_args.size() && i < call_state.locals.size(); ++i) {
                    call_state.locals[i] = std::move(lowered_args[i]);
                }
                return call_state.run();
            }
            case Opcode::Return: {
                Value result = evalOperand();
                pc = unit->bytecode.size();
                return result;
            }
            case Opcode::Let: {
                OperandType dest_type = static_cast<OperandType>(read8());

                switch (dest_type) {
                    case OperandType::StackLocal: {
                        uint32_t slot = read32();
                        Value value = evalOperand();
                        locals.at(slot) = value;
                        return value;
                    }
                    case OperandType::Identifier: {
                        std::string name = unit->constant_strings.at(read32());
                        Value value = evalOperand();
                        globals[name] = value;
                        return value;
                    }
                    default:
                        throw std::runtime_error("Let instruction requires a local or global destination");
                }
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

} // namespace

Value Evaluator::evaluate(std::shared_ptr<ProgramUnit> unit, std::unordered_map<std::string, Value>& globals, std::ostream& out) {
    ExecutionState state(std::move(unit), globals, out);
    return state.run();
}

} // namespace chirp::vm
