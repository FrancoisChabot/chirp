#include "evaluator.h"

#include "opcodes.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace chirp::vm {

namespace {

struct BreakSignal {
    Value value;
};

struct BindingSlot {
    Value value;
    bool initialized = false;
};

class ExecutionState {
public:
    std::shared_ptr<ProgramUnit> unit;
    size_t pc = 0;
    std::vector<BindingSlot> locals;
    const std::vector<Value>* captures = nullptr;
    std::unordered_map<std::string, Value>& globals;
    std::ostream& out;
    const std::unordered_map<std::string, Value>* registry = nullptr;
    const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls = nullptr;

    ExecutionState(std::shared_ptr<ProgramUnit> current_unit,
                   std::unordered_map<std::string, Value>& global_values,
                   std::ostream& output,
                   const std::unordered_map<std::string, Value>* registered_values = nullptr,
                   const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* registered_trait_impls = nullptr,
                   const std::vector<Value>* captured_values = nullptr)
    : unit(std::move(current_unit)),
          locals(unit->num_locals),
          captures(captured_values),
          globals(global_values),
          out(output),
          registry(registered_values),
          trait_impls(registered_trait_impls) {}

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

    Value evalOperandAt(size_t start, size_t end) {
        size_t saved_pc = pc;
        pc = start;
        Value result = evalOperand();
        if (pc != end) {
            throw std::runtime_error("Inline operand boundary mismatch");
        }
        pc = saved_pc;
        return result;
    }

    static bool valueEquals(const Value& left, const Value& right) {
        if (left.type != right.type) {
            return false;
        }

        switch (left.type) {
            case ValueType::Null:
                return true;
            case ValueType::Int:
            case ValueType::Bool:
            case ValueType::Char:
                return left.as_int == right.as_int;
            case ValueType::String:
            case ValueType::Symbol:
                return left.as_string == right.as_string;
            case ValueType::StructType:
                return left.as_struct_type == right.as_struct_type;
            case ValueType::TypeValue:
                return left.as_type_value == right.as_type_value;
            case ValueType::Minted:
                return left.as_type_value == right.as_type_value && left.as_id == right.as_id;
            case ValueType::Trait:
                return left.as_trait == right.as_trait;
            case ValueType::Closure:
                return left.as_closure == right.as_closure;
            case ValueType::NativeFunc:
                return left.as_native == right.as_native;
            case ValueType::Signature:
                return left.as_signature == right.as_signature;
            case ValueType::EnumeratedSet:
                if (left.as_set_elements->size() != right.as_set_elements->size()) {
                    return false;
                }
                for (size_t i = 0; i < left.as_set_elements->size(); ++i) {
                    if (!valueEquals(left.as_set_elements->at(i), right.as_set_elements->at(i))) {
                        return false;
                    }
                }
                return true;
            case ValueType::Struct:
                if (left.as_struct_instance_type != right.as_struct_instance_type ||
                    left.as_struct->size() != right.as_struct->size()) {
                    return false;
                }
                for (const auto& [name, value] : *left.as_struct) {
                    auto it = right.as_struct->find(name);
                    if (it == right.as_struct->end() || !valueEquals(value, it->second)) {
                        return false;
                    }
                }
                return true;
            case ValueType::Array:
                if (left.as_array->size() != right.as_array->size()) {
                    return false;
                }
                for (size_t i = 0; i < left.as_array->size(); ++i) {
                    if (!valueEquals(left.as_array->at(i), right.as_array->at(i))) {
                        return false;
                    }
                }
                return true;
            case ValueType::ConstructedSet:
                return left.as_constructed_set == right.as_constructed_set;
            case ValueType::CompositeSet:
                return left.as_composite_set == right.as_composite_set;
            case ValueType::Heap:
                return left.as_heap == right.as_heap;
            case ValueType::EnumFamily:
                return left.as_enum_family->node_id == right.as_enum_family->node_id;
            case ValueType::EnumVariant:
                return left.as_enum_variant->enum_node_id == right.as_enum_variant->enum_node_id && 
                       left.as_enum_variant->index == right.as_enum_variant->index;
        }
        return false;
    }

    static bool isTruthy(const Value& value) {
        if (value.type == ValueType::Bool || value.type == ValueType::Int) {
            return value.as_int != BigInt(0);
        }
        return false;
    }

    Value typeOf(const Value& value) {
        if (value.type == ValueType::Null) return globals.at("__void_type");
        if (value.type == ValueType::Int) return globals.at("int");
        if (value.type == ValueType::Bool) return globals.at("bool");
        if (value.type == ValueType::String) return globals.at("string");
        if (value.type == ValueType::Char) return globals.at("char");
        if (value.type == ValueType::Symbol) return globals.at("symbol");
        if (value.type == ValueType::Struct && value.as_struct_instance_type) return Value::StructType(value.as_struct_instance_type);
        if (value.type == ValueType::StructType || value.type == ValueType::TypeValue) return globals.at("type");
        if (value.type == ValueType::Closure || value.type == ValueType::NativeFunc) return globals.at("lambda");
        if (value.type == ValueType::Trait) return globals.at("trait");
        if (value.type == ValueType::Minted) return Value::Type(value.as_type_value);
        if (value.type == ValueType::Heap) return Value::Type(value.as_type_value);
        if (value.type == ValueType::EnumVariant) return globals.at("EnumVariant");
        return Value::Symbol("unknown");
    }

    bool isSharedHeapAllocation(const Value& value) const {
        if (value.type != ValueType::Heap) {
            return false;
        }
        auto it = globals.find("__heap_shared_allocation_type");
        return it != globals.end() &&
            it->second.type == ValueType::TypeValue &&
            value.as_type_value == it->second.as_type_value;
    }

    void retainOwnedValue(const Value& value) {
        if (!isSharedHeapAllocation(value)) {
            return;
        }
        if (value.as_heap == nullptr || value.as_heap->destroyed) {
            throw std::runtime_error("Cannot retain destroyed shared heap allocation");
        }
        ++value.as_heap->strong_count;
    }

    static int64_t requireInt64(const BigInt& value, const std::string& message) {
        if (!value.fits_int64()) {
            throw std::runtime_error(message);
        }
        return value.to_int64();
    }

    static std::string typeKey(const Value& type) {
        auto pointerKey = [](const void* ptr) {
            std::ostringstream oss;
            oss << ptr;
            return oss.str();
        };
        switch (type.type) {
            case ValueType::TypeValue:
                return "type:" + pointerKey(type.as_type_value.get());
            case ValueType::StructType:
                return "struct:" + pointerKey(type.as_struct_type.get());
            default:
                throw std::runtime_error("Expected a type value");
        }
    }

    const Value* registeredItem(const std::string& key) const {
        if (registry == nullptr) {
            return nullptr;
        }
        auto it = registry->find(key);
        if (it == registry->end()) {
            return nullptr;
        }
        return &it->second;
    }

    const Value* registeredImplementation(const Value& trait, const Value& subject_type) const {
        if (trait_impls == nullptr ||
            trait.type != ValueType::Trait ||
            (subject_type.type != ValueType::TypeValue && subject_type.type != ValueType::StructType)) {
            return nullptr;
        }
        auto trait_it = trait_impls->find(trait.as_trait->id);
        if (trait_it == trait_impls->end()) {
            return nullptr;
        }
        auto impl_it = trait_it->second.find(typeKey(subject_type));
        if (impl_it == trait_it->second.end()) {
            return nullptr;
        }
        return &impl_it->second;
    }

    const Value* structField(const Value& value, const std::string& name) const {
        if (value.type != ValueType::Struct || value.as_struct == nullptr) {
            return nullptr;
        }
        auto it = value.as_struct->find(name);
        if (it == value.as_struct->end()) {
            return nullptr;
        }
        return &it->second;
    }

    void dropValue(const Value& value) {
        const Value* drop_trait = registeredItem("traits.drop");
        if (drop_trait == nullptr) {
            return;
        }
        const Value* impl = registeredImplementation(*drop_trait, typeOf(value));
        if (impl == nullptr) {
            return;
        }
        const Value* drop_fn = structField(*impl, "drop");
        if (drop_fn == nullptr) {
            throw std::runtime_error("Drop implementation missing drop");
        }
        Value result = invokeValue(*drop_fn, {CallArgument{std::nullopt, value}});
        if (result.type != ValueType::Null) {
            throw std::runtime_error("Drop implementation must return void");
        }
    }

    Value readLocal(uint32_t slot) const {
        const auto& local = locals.at(slot);
        return local.initialized ? local.value : Value();
    }

    void assignLocal(uint32_t slot, Value value) {
        retainOwnedValue(value);
        auto& local = locals.at(slot);
        if (local.initialized) {
            dropValue(local.value);
        }
        local.value = std::move(value);
        local.initialized = true;
    }

    void dropLocal(uint32_t slot) {
        auto& local = locals.at(slot);
        if (!local.initialized) {
            return;
        }
        dropValue(local.value);
        local.value = Value();
        local.initialized = false;
    }

    void assignCapture(uint32_t slot, Value value) {
        if (captures == nullptr) {
            throw std::runtime_error("Assignment to capture with no closure environment");
        }
        retainOwnedValue(value);
        auto* mutable_captures = const_cast<std::vector<Value>*>(captures);
        dropValue(mutable_captures->at(slot));
        mutable_captures->at(slot) = std::move(value);
    }

    void dropCapture(uint32_t slot) {
        if (captures == nullptr) {
            throw std::runtime_error("Drop of capture with no closure environment");
        }
        auto* mutable_captures = const_cast<std::vector<Value>*>(captures);
        dropValue(mutable_captures->at(slot));
        mutable_captures->at(slot) = Value();
    }

    void assignGlobal(const std::string& name, Value value) {
        retainOwnedValue(value);
        auto found = globals.find(name);
        if (found != globals.end()) {
            dropValue(found->second);
            found->second = std::move(value);
            return;
        }
        globals.emplace(name, std::move(value));
    }

    void dropGlobal(const std::string& name) {
        auto found = globals.find(name);
        if (found == globals.end()) {
            return;
        }
        dropValue(found->second);
        globals.erase(found);
    }

    Value evalOperand() {
        OperandType type = static_cast<OperandType>(read8());
        switch (type) {
            case OperandType::ImmInt:
                return Value(unit->constant_ints.at(read32()));
            case OperandType::ImmString:
                return Value(unit->constant_strings.at(read32()));
            case OperandType::ImmChar:
                return Value::Char(read32());
            case OperandType::ImmSymbol:
                return Value::Symbol(unit->constant_strings.at(read32()));
            case OperandType::ImmNull:
                return Value();
            case OperandType::ImmBool:
                return Value(read8() != 0);
            case OperandType::Inline:
                return evalInstruction();
            case OperandType::StackLocal:
                return readLocal(read32());
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
                throw std::runtime_error("Unsupported operand type in evalOperand: " + std::to_string(static_cast<int>(type)));
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

    std::shared_ptr<Closure> captureChildClosure(uint32_t child_index) {
        auto child = unit->child_units.at(child_index);
        std::vector<Value> captured_values;
        captured_values.reserve(child->captures.size());
        for (const auto& capture : child->captures) {
            switch (capture.kind) {
                case CaptureSourceKind::Local:
                    captured_values.push_back(readLocal(capture.index));
                    break;
                case CaptureSourceKind::Capture:
                    if (captures == nullptr) {
                        throw std::runtime_error("Nested capture read with no closure environment");
                    }
                    captured_values.push_back(captures->at(capture.index));
                    break;
            }
            retainOwnedValue(captured_values.back());
        }
        return std::make_shared<Closure>(Closure{std::move(child), std::move(captured_values)});
    }

    Value invokeClosure(const Closure& closure, std::vector<Value> args = {}) {
        ExecutionState call_state(closure.unit, globals, out, registry, trait_impls, &closure.captures);
        for (size_t i = 0; i < args.size() && i < call_state.locals.size(); ++i) {
            call_state.locals[i].value = std::move(args[i]);
            call_state.locals[i].initialized = true;
        }
        return call_state.run();
    }

    Value buildArgumentStruct(const Value& impl,
                              const std::string& param_space_field,
                              const Value& self,
                              const std::vector<CallArgument>& args) {
        const Value* param_space_fn = structField(impl, param_space_field);
        if (param_space_fn == nullptr) {
            throw std::runtime_error("Trait implementation missing field: " + param_space_field);
        }
        Value param_space = invokeValue(*param_space_fn, {CallArgument{std::nullopt, self}});
        if (param_space.type != ValueType::StructType) {
            throw std::runtime_error("Trait parameter space must be a struct type");
        }
        return constructStruct(param_space.as_struct_type, args);
    }

    Value invokeValue(const Value& callee, const std::vector<CallArgument>& args) {
        if (callee.type == ValueType::NativeFunc) {
            return (*callee.as_native)(args);
        }
        if (callee.type == ValueType::StructType) {
            return constructStruct(callee.as_struct_type, args);
        }
        if (callee.type == ValueType::Closure) {
            std::vector<Value> lowered_args = lower_closure_arguments(*callee.as_closure, args);
            return invokeClosure(*callee.as_closure, std::move(lowered_args));
        }

        if (const Value* callable_trait = registeredItem("traits.callable")) {
            if (const Value* impl = registeredImplementation(*callable_trait, typeOf(callee))) {
                const Value* invoke_fn = structField(*impl, "invoke");
                if (invoke_fn == nullptr) {
                    throw std::runtime_error("Callable implementation missing invoke");
                }
                Value params = buildArgumentStruct(*impl, "param_space", callee, args);
                return invokeValue(*invoke_fn, {
                    CallArgument{std::nullopt, callee},
                    CallArgument{std::nullopt, std::move(params)},
                });
            }
        }

        throw std::runtime_error("Type error: target is not callable");
    }

    Value dereferenceValue(const Value& value) {
        if (value.type == ValueType::Heap) {
            if (value.as_heap == nullptr || value.as_heap->destroyed) {
                throw std::runtime_error("Cannot dereference destroyed heap allocation");
            }
            return value.as_heap->stored;
        }
        if (const Value* dereferenceable_trait = registeredItem("traits.dereferenceable")) {
            if (const Value* impl = registeredImplementation(*dereferenceable_trait, typeOf(value))) {
                const Value* deref_fn = structField(*impl, "deref");
                if (deref_fn == nullptr) {
                    throw std::runtime_error("Dereferenceable implementation missing deref");
                }
                return invokeValue(*deref_fn, {CallArgument{std::nullopt, value}});
            }
        }
        throw std::runtime_error("Cannot dereference non-pointer value");
    }

    Value storeDereference(const Value& pointer, Value new_value) {
        if (pointer.type == ValueType::Heap) {
            if (pointer.as_heap == nullptr || pointer.as_heap->destroyed) {
                throw std::runtime_error("Cannot assign through destroyed heap allocation");
            }
            retainOwnedValue(new_value);
            Value old_value = pointer.as_heap->stored;
            dropValue(old_value);
            pointer.as_heap->stored = new_value;
            return new_value;
        }
        if (const Value* dereferenceable_mut_trait = registeredItem("traits.dereferenceable_mut")) {
            if (const Value* impl = registeredImplementation(*dereferenceable_mut_trait, typeOf(pointer))) {
                const Value* assign_fn = structField(*impl, "deref_assign");
                if (assign_fn == nullptr) {
                    throw std::runtime_error("DereferenceableMut implementation missing deref_assign");
                }
                return invokeValue(*assign_fn, {
                    CallArgument{std::nullopt, pointer},
                    CallArgument{std::nullopt, new_value},
                });
            }
        }
        throw std::runtime_error("Cannot assign through non-pointer value");
    }

    static bool primitive_constraint_matches(const std::shared_ptr<TypeValueDef>& constraint_type, const Value& value) {
        if (!constraint_type) {
            return false;
        }
        if (constraint_type->kind != TypeValueDef::Kind::Primitive) {
            return false;
        }
        if (constraint_type->name == "int") return value.type == ValueType::Int;
        if (constraint_type->name == "bool") return value.type == ValueType::Bool;
        if (constraint_type->name == "string") return value.type == ValueType::String;
        if (constraint_type->name == "char") return value.type == ValueType::Char;
        if (constraint_type->name == "symbol") return value.type == ValueType::Symbol;
        if (constraint_type->name == "void") return value.type == ValueType::Null;
        if (constraint_type->name == "any") return true;
        return false;
    }

    static void appendUnique(std::vector<Value>& values, const Value& candidate) {
        auto found = std::find_if(values.begin(), values.end(), [&](const Value& existing) {
            return valueEquals(existing, candidate);
        });
        if (found == values.end()) {
            values.push_back(candidate);
        }
    }

    Value evaluateCompare(CompareOp cmp_op, const Value& left, const Value& right) {
        if ((left.type == ValueType::Int && right.type == ValueType::Int) ||
            (left.type == ValueType::Char && right.type == ValueType::Char)) {
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

        if (cmp_op == CompareOp::Eq) {
            return Value(valueEquals(left, right));
        }
        if (cmp_op == CompareOp::Neq) {
            return Value(!valueEquals(left, right));
        }

        if (left.type == ValueType::EnumVariant && right.type == ValueType::EnumVariant) {
            if (left.as_enum_variant->enum_node_id == right.as_enum_variant->enum_node_id) {
                switch (cmp_op) {
                    case CompareOp::Lt: return Value(left.as_enum_variant->index < right.as_enum_variant->index);
                    case CompareOp::Lte: return Value(left.as_enum_variant->index <= right.as_enum_variant->index);
                    case CompareOp::Gt: return Value(left.as_enum_variant->index > right.as_enum_variant->index);
                    case CompareOp::Gte: return Value(left.as_enum_variant->index >= right.as_enum_variant->index);
                    default: break;
                }
            } else {
                throw std::runtime_error("Cannot order variants from different enum families");
            }
        }

        if (const Value* comparable_trait = registeredItem("operators.comparable")) {
            if (const Value* impl = registeredImplementation(*comparable_trait, typeOf(left))) {
                const Value* compare_fn = structField(*impl, "compare");
                if (compare_fn == nullptr) {
                    throw std::runtime_error("Comparable implementation missing compare");
                }
                Value result = invokeValue(*compare_fn, {
                    CallArgument{std::nullopt, left},
                    CallArgument{std::nullopt, right},
                });
                const Value* less = registeredItem("operators.comparable.less");
                const Value* equal = registeredItem("operators.comparable.equal");
                const Value* greater = registeredItem("operators.comparable.greater");
                if (less == nullptr || equal == nullptr || greater == nullptr) {
                    throw std::runtime_error("Comparable registry is incomplete");
                }

                switch (cmp_op) {
                    case CompareOp::Lt: return Value(valueEquals(result, *less));
                    case CompareOp::Lte: return Value(valueEquals(result, *less) || valueEquals(result, *equal));
                    case CompareOp::Gt: return Value(valueEquals(result, *greater));
                    case CompareOp::Gte: return Value(valueEquals(result, *greater) || valueEquals(result, *equal));
                    default:
                        throw std::runtime_error("Unknown CompareOp");
                }
            }
        }
        throw std::runtime_error("Types cannot be compared");
    }

    std::vector<Value> finiteElements(const Value& set) {
        if (set.type == ValueType::EnumFamily) {
            std::vector<Value> elements;
            for (size_t i = 0; i < set.as_enum_family->variants.size(); ++i) {
                auto variant_def = std::make_shared<EnumVariantDef>();
                variant_def->enum_node_id = set.as_enum_family->node_id;
                variant_def->variant_name = set.as_enum_family->variants[i];
                variant_def->index = i;
                elements.push_back(Value::EnumVariant(std::move(variant_def)));
            }
            return elements;
        }

        if (set.type == ValueType::EnumeratedSet) {
            std::vector<Value> elements;
            for (const auto& value : *set.as_set_elements) {
                appendUnique(elements, value);
            }
            return elements;
        }

        if (set.type == ValueType::Array) {
            return *set.as_array;
        }

        if (set.type == ValueType::TypeValue &&
            set.as_type_value &&
            set.as_type_value->kind == TypeValueDef::Kind::Finite) {
            std::vector<Value> elements;
            for (uint64_t i = 0; i < set.as_type_value->finite_count; ++i) {
                elements.push_back(Value::Minted(set.as_type_value, i));
            }
            return elements;
        }

        if (set.type == ValueType::CompositeSet) {
            std::vector<Value> left = finiteElements(*set.as_composite_set->left);
            std::vector<Value> right = finiteElements(*set.as_composite_set->right);
            if (set.as_composite_set->op == CompositeSetDef::Op::Union) {
                for (const auto& value : right) {
                    appendUnique(left, value);
                }
                return left;
            }

            std::vector<Value> result;
            for (const auto& value : left) {
                if (isTruthy(belongsTo(*set.as_composite_set->right, value))) {
                    appendUnique(result, value);
                }
            }
            return result;
        }

        if (set.type == ValueType::ConstructedSet) {
            if (!set.as_constructed_set->bound) {
                throw std::runtime_error("Cannot materialize unbounded constructed set");
            }
            Value bound = invokeClosure(*set.as_constructed_set->bound);
            std::vector<Value> candidates = finiteElements(bound);
            std::vector<Value> result;
            for (const auto& candidate : candidates) {
                if (isTruthy(belongsTo(set, candidate))) {
                    appendUnique(result, candidate);
                }
            }
            return result;
        }

        if (set.type == ValueType::Range) {
            if (set.as_range->start->type == ValueType::Int && set.as_range->end->type == ValueType::Int) {
                std::vector<Value> elements;
                int64_t start = requireInt64(set.as_range->start->as_int, "Range bounds must fit in int64");
                int64_t end = requireInt64(set.as_range->end->as_int, "Range bounds must fit in int64");
                if (set.as_range->inclusive_end) {
                    for (int64_t i = start; i <= end; ++i) elements.push_back(Value(i));
                } else {
                    for (int64_t i = start; i < end; ++i) elements.push_back(Value(i));
                }
                return elements;
            }
            if (set.as_range->start->type == ValueType::Char && set.as_range->end->type == ValueType::Char) {
                std::vector<Value> elements;
                uint32_t start = static_cast<uint32_t>(set.as_range->start->as_int.to_int64());
                uint32_t end = static_cast<uint32_t>(set.as_range->end->as_int.to_int64());
                if (set.as_range->inclusive_end) {
                    for (uint32_t i = start; i <= end; ++i) elements.push_back(Value::Char(i));
                } else {
                    for (uint32_t i = start; i < end; ++i) elements.push_back(Value::Char(i));
                }
                return elements;
            }
            throw std::runtime_error("Can only enumerate integer and char ranges");
        }

        throw std::runtime_error("Set is not finitely enumerable");
    }

    Value belongsTo(const Value& set, const Value& value) {
        switch (set.type) {
            case ValueType::EnumFamily:
                if (value.type != ValueType::EnumVariant) return Value(false);
                return Value(value.as_enum_variant->enum_node_id == set.as_enum_family->node_id);
            case ValueType::EnumVariant:
                return Value(valueEquals(set, value));
            case ValueType::TypeValue:
                return Value(valueEquals(typeOf(value), set));
            case ValueType::StructType:
                return Value(value.type == ValueType::Struct && value.as_struct_instance_type == set.as_struct_type);
            case ValueType::Signature:
                if (value.type == ValueType::Closure) {
                    return Value(value.as_closure->unit->parameter_names.size() == set.as_signature->parameters.size());
                }
                if (value.type == ValueType::NativeFunc) {
                    return Value(true);
                }
                return Value(false);
            case ValueType::EnumeratedSet: {
                for (const auto& element : *set.as_set_elements) {
                    if (valueEquals(element, value)) {
                        return Value(true);
                    }
                }
                return Value(false);
            }
            case ValueType::ComplementSet: {
                Value inner_result = belongsTo(*set.as_complement_set->inner, value);
                if (inner_result.type != ValueType::Bool) {
                    throw std::runtime_error("Complement inner set must evaluate to Bool");
                }
                return Value(!isTruthy(inner_result));
            }
            case ValueType::ConstructedSet: {
                if (set.as_constructed_set->bound) {
                    Value in_bound = belongsTo(invokeClosure(*set.as_constructed_set->bound), value);
                    if (!isTruthy(in_bound)) {
                        return Value(false);
                    }
                }
                Value predicate_result = invokeClosure(*set.as_constructed_set->predicate, {value});
                if (predicate_result.type != ValueType::Bool) {
                    throw std::runtime_error("Constructed set predicate must evaluate to Bool");
                }
                return predicate_result;
            }
            case ValueType::CompositeSet: {
                Value left = belongsTo(*set.as_composite_set->left, value);
                Value right = belongsTo(*set.as_composite_set->right, value);
                if (set.as_composite_set->op == CompositeSetDef::Op::Union) {
                    return Value(isTruthy(left) || isTruthy(right));
                }
                return Value(isTruthy(left) && isTruthy(right));
            }
            case ValueType::Range: {
                if (value.type != set.as_range->start->type) return Value(false);
                
                bool gte = isTruthy(evaluateCompare(CompareOp::Gte, value, *set.as_range->start));
                bool lte = set.as_range->inclusive_end
                             ? isTruthy(evaluateCompare(CompareOp::Lte, value, *set.as_range->end))
                             : isTruthy(evaluateCompare(CompareOp::Lt, value, *set.as_range->end));
                return Value(gte && lte);
            }
            default:
                if (const Value* set_trait = registeredItem("traits.set")) {
                    if (const Value* impl = registeredImplementation(*set_trait, typeOf(set))) {
                        const Value* belongs_fn = structField(*impl, "belongs");
                        if (belongs_fn == nullptr) {
                            throw std::runtime_error("Set implementation missing belongs");
                        }
                        return invokeValue(*belongs_fn, {
                            CallArgument{std::nullopt, set},
                            CallArgument{std::nullopt, value},
                        });
                    }
                }
                throw std::runtime_error("Expected set operand: set type " + std::to_string(static_cast<int>(set.type)) + " value type " + std::to_string(static_cast<int>(value.type)));
        }
    }

    Value enforceConstraint(Value constraint, Value value, const std::string& field_name) {
        if (constraint.type == ValueType::Null) {
            return value;
        }

        if (constraint.type == ValueType::StructType) {
            if (value.type == ValueType::Struct && value.as_struct_instance_type == nullptr) {
                std::vector<CallArgument> args;
                args.reserve(value.as_struct->size());
                for (const auto& [name, field_value] : *value.as_struct) {
                    args.push_back(CallArgument{std::optional<std::string>(name), field_value});
                }
                return constructStruct(constraint.as_struct_type, args);
            }
        }

        if (!isTruthy(belongsTo(constraint, value))) {
            throw std::runtime_error("Constraint failure for '" + field_name + "'");
        }
        return value;
    }

    static std::vector<CallArgument> require_consistent_argument_style(const std::vector<CallArgument>& args) {
        reject_mixed_call_arguments(args);
        return args;
    }

    Value constructStruct(const std::shared_ptr<StructTypeDef>& type_def, const std::vector<CallArgument>& args) {
        require_consistent_argument_style(args);

        bool has_named = false;
        for (const auto& arg : args) {
            has_named = has_named || arg.name.has_value();
        }

        if (!has_named && args.size() > type_def->fields.size()) {
            throw std::runtime_error("Too many positional arguments for struct constructor");
        }

        std::unordered_map<std::string, Value> provided;
        if (has_named) {
            for (const auto& arg : args) {
                const std::string& name = *arg.name;
                if (provided.contains(name)) {
                    throw std::runtime_error("Duplicate named argument: " + name);
                }
                provided.emplace(name, arg.value);
            }
        } else {
            for (size_t i = 0; i < args.size(); ++i) {
                provided.emplace(type_def->fields[i].name, args[i].value);
            }
        }

        auto instance = std::make_shared<std::unordered_map<std::string, Value>>();
        for (const auto& pair : provided) {
            bool found = false;
            for (const auto& field : type_def->fields) {
                if (field.name == pair.first) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Unknown field in struct constructor: " + pair.first);
            }
        }

        for (const auto& field : type_def->fields) {
            auto provided_it = provided.find(field.name);
            if (provided_it != provided.end()) {
                Value field_value = provided_it->second;
                if (field.constraint) {
                    field_value = enforceConstraint(invokeClosure(*field.constraint), std::move(field_value), field.name);
                }
                (*instance)[field.name] = std::move(field_value);
                continue;
            }
            if (field.default_value) {
                Value field_value = invokeClosure(*field.default_value);
                if (field.constraint) {
                    field_value = enforceConstraint(invokeClosure(*field.constraint), std::move(field_value), field.name);
                }
                (*instance)[field.name] = std::move(field_value);
                continue;
            }
            throw std::runtime_error("Missing required field: " + field.name);
        }
        return Value::Struct(instance, type_def);
    }

    Value evalInstruction() {
        if (pc >= unit->bytecode.size()) {
            throw std::runtime_error("PC out of bounds");
        }
        Opcode op = decodeOpcode(unit->bytecode[pc]);
        pc++;
        Domain dom = decodeDomain(unit->bytecode[pc - 1]);

        switch (op) {
            case Opcode::Eval:
                return evalOperand();
            case Opcode::Block: {
                uint32_t block_len = read32();
                size_t end_pc = pc + block_len;
                try {
                    while (pc < end_pc) {
                        evalInstruction();
                    }
                    return Value();
                } catch (BreakSignal& signal) {
                    pc = end_pc;
                    return std::move(signal.value);
                }
            }
            case Opcode::Break:
            {
                Value result = evalOperand();
                uint32_t cleanup_count = read32();
                for (uint32_t i = 0; i < cleanup_count; ++i) {
                    OperandType dest_type = static_cast<OperandType>(read8());
                    switch (dest_type) {
                        case OperandType::StackLocal:
                            dropLocal(read32());
                            break;
                        case OperandType::Capture:
                            dropCapture(read32());
                            break;
                        case OperandType::Identifier:
                            dropGlobal(unit->constant_strings.at(read32()));
                            break;
                        default:
                            throw std::runtime_error("Break cleanup requires a local, capture, or global destination");
                    }
                }
                throw BreakSignal{std::move(result)};
            }
            case Opcode::Loop: {
                uint32_t loop_len = read32();
                size_t loop_start_pc = pc;
                try {
                    while (true) {
                        pc = loop_start_pc;
                        evalOperand();
                    }
                } catch (BreakSignal& signal) {
                    pc = loop_start_pc + loop_len;
                    return std::move(signal.value);
                }
            }
            case Opcode::GetField: {
                Value target = evalOperand();
                Value field = evalOperand();
                if (target.type == ValueType::EnumFamily) {
                    const auto& variants = target.as_enum_family->variants;
                    for (size_t i = 0; i < variants.size(); ++i) {
                        if (variants[i] == field.as_string) {
                            auto variant_def = std::make_shared<EnumVariantDef>();
                            variant_def->enum_node_id = target.as_enum_family->node_id;
                            variant_def->variant_name = field.as_string;
                            variant_def->index = i;
                            return Value::EnumVariant(std::move(variant_def));
                        }
                    }
                    throw std::runtime_error("Enum missing variant: " + field.as_string);
                }
                if (target.type != ValueType::Struct) {
                    throw std::runtime_error("GetField target must be a struct or enum");
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
                    if (!index.as_int.fits_int64()) {
                        throw std::runtime_error("Array index out of bounds");
                    }
                    int64_t index_value = index.as_int.to_int64();
                    if (index_value < 0 || static_cast<size_t>(index_value) >= target.as_array->size()) {
                        throw std::runtime_error("Array index out of bounds");
                    }
                    return target.as_array->at(static_cast<size_t>(index_value));
                }
                if (const Value* indexable_trait = registeredItem("operators.indexable")) {
                    if (const Value* impl = registeredImplementation(*indexable_trait, typeOf(target))) {
                        const Value* read_fn = structField(*impl, "read");
                        if (read_fn == nullptr) {
                            throw std::runtime_error("Indexable implementation missing read");
                        }
                        Value at = buildArgumentStruct(
                            *impl,
                            "index_space",
                            target,
                            {CallArgument{std::nullopt, index}});
                        return invokeValue(*read_fn, {
                            CallArgument{std::nullopt, target},
                            CallArgument{std::nullopt, std::move(at)},
                        });
                    }
                }
                throw std::runtime_error("Index operation on non-array type");
            }
            case Opcode::Contains: {
                Value value = evalOperand();
                Value set = evalOperand();
                return belongsTo(set, value);
            }
            case Opcode::MakeRange: {
                bool inclusive_end = read8() != 0;
                Value start = evalOperand();
                Value end = evalOperand();
                return Value::Range(std::make_shared<Value>(std::move(start)), std::make_shared<Value>(std::move(end)), inclusive_end);
            }
            case Opcode::Deref:
                return dereferenceValue(evalOperand());
            case Opcode::Not:
            case Opcode::Negate:
            case Opcode::Complement: {
                Value right = evalOperand();
                
                if (op == Opcode::Not) {
                    return Value(!isTruthy(right));
                }

                if (right.type == ValueType::Int) {
                    switch (op) {
                        case Opcode::Negate:
                            return Value(-right.as_int);
                        case Opcode::Complement:
                            return Value(~requireInt64(right.as_int, "Type error: integer does not fit in int64 for complement"));
                        default:
                            throw std::runtime_error("Unknown unary math op");
                    }
                }

                if (op == Opcode::Negate) {
                    if (const Value* negatable_trait = registeredItem("operators.negatable")) {
                        if (const Value* impl = registeredImplementation(*negatable_trait, typeOf(right))) {
                            const Value* neg_fn = structField(*impl, "neg");
                            if (neg_fn == nullptr) {
                                throw std::runtime_error("Negatable implementation missing neg");
                            }
                            return invokeValue(*neg_fn, {CallArgument{std::nullopt, right}});
                        }
                    }
                }
                
                if (op == Opcode::Complement) {
                    auto comp = std::make_shared<ComplementSetDef>();
                    comp->inner = std::make_shared<Value>(right);
                    return Value::ComplementSet(comp);
                }

                throw std::runtime_error("Type error: expected integer for unary math");
            }
            case Opcode::Add:
            case Opcode::Sub:
            case Opcode::Mul:
            case Opcode::Div:
            case Opcode::Mod: {
                Value left = evalOperand();
                Value right = evalOperand();

                if (dom != Domain::Generic) {
                    throw std::runtime_error("Specialized domains are not implemented");
                }
                if (left.type == ValueType::Int && right.type == ValueType::Int) {
                    switch (op) {
                        case Opcode::Add: return Value(left.as_int + right.as_int);
                        case Opcode::Sub: return Value(left.as_int - right.as_int);
                        case Opcode::Mul: return Value(left.as_int * right.as_int);
                        case Opcode::Div:
                            if (right.as_int == BigInt(0)) throw std::runtime_error("Division by zero");
                            return Value(left.as_int / right.as_int);
                        case Opcode::Mod:
                            if (right.as_int == BigInt(0)) throw std::runtime_error("Modulo by zero");
                            return Value(left.as_int % right.as_int);
                        default:
                            throw std::runtime_error("Unknown BinaryOp");
                    }
                }

                const char* trait_key = nullptr;
                const char* method_name = nullptr;
                switch (op) {
                    case Opcode::Add:
                        trait_key = "operators.additive";
                        method_name = "add";
                        break;
                    case Opcode::Sub:
                        trait_key = "operators.subtractive";
                        method_name = "sub";
                        break;
                    case Opcode::Mul:
                        trait_key = "operators.multiplicative";
                        method_name = "mul";
                        break;
                    case Opcode::Div:
                        trait_key = "operators.divisible";
                        method_name = "div";
                        break;
                    case Opcode::Mod:
                        trait_key = "operators.modulable";
                        method_name = "mod";
                        break;
                    default:
                        break;
                }

                if (const Value* trait = registeredItem(trait_key)) {
                    if (const Value* impl = registeredImplementation(*trait, typeOf(left))) {
                        const Value* method = structField(*impl, method_name);
                        if (method == nullptr) {
                            throw std::runtime_error(std::string("Trait implementation missing field: ") + method_name);
                        }
                        return invokeValue(*method, {
                            CallArgument{std::nullopt, left},
                            CallArgument{std::nullopt, right},
                        });
                    }
                }

                throw std::runtime_error("Type error: expected integers for math");
            }
            case Opcode::Compare: {
                CompareOp cmp_op = static_cast<CompareOp>(read8());
                Value left = evalOperand();
                Value right = evalOperand();

                if (dom != Domain::Generic) {
                    throw std::runtime_error("Specialized domains are not implemented");
                }
                return evaluateCompare(cmp_op, left, right);
            }
            case Opcode::If: {
                Value cond = evalOperand();
                uint32_t true_len = read32();

                if (isTruthy(cond)) {
                    Value result = evalOperand();
                    pc += read32();
                    return result;
                }

                pc += true_len;
                [[maybe_unused]] uint32_t false_len = read32();
                return evalOperand();
            }
            case Opcode::Match: {
                uint32_t total_match_len = read32();
                size_t match_start_pc = pc;
                uint8_t num_arms = read8();
                Value subject = evalOperand();

                Value result;
                bool matched = false;

                for (uint8_t i = 0; i < num_arms; ++i) {
                    uint32_t header = read32();
                    uint32_t result_len = header & 0xFFFFFF;
                    pc--; // Rewind PC to the 8-bit condition operand header
                    
                    Value cond = evalOperand();
                    Value matches = belongsTo(cond, subject);

                    if (isTruthy(matches)) {
                        result = evalOperand();
                        pc = match_start_pc + total_match_len;
                        matched = true;
                        break;
                    } else {
                        pc += result_len;
                    }
                }
                
                if (!matched) {
                    throw std::runtime_error("Match expression failed: no branches matched");
                }
                return result;
            }
            case Opcode::MakeLambda: {
                return Value(captureChildClosure(read32()));
            }
            case Opcode::Call: {
                Value callee = evalOperand();
                std::vector<CallArgument> args = readCallArguments();
                return invokeValue(callee, args);
            }
            case Opcode::Return: {
                Value result = evalOperand();
                pc = unit->bytecode.size();
                return result;
            }
            case Opcode::Union: {
                Value left = evalOperand();
                Value right = evalOperand();
                return Value::CompositeSet(std::make_shared<CompositeSetDef>(CompositeSetDef{
                    std::make_shared<Value>(std::move(left)),
                    std::make_shared<Value>(std::move(right)),
                    CompositeSetDef::Op::Union,
                }));
            }
            case Opcode::Intersect: {
                Value left = evalOperand();
                Value right = evalOperand();
                return Value::CompositeSet(std::make_shared<CompositeSetDef>(CompositeSetDef{
                    std::make_shared<Value>(std::move(left)),
                    std::make_shared<Value>(std::move(right)),
                    CompositeSetDef::Op::Intersection,
                }));
            }
            case Opcode::MakeStructDef: {
                uint32_t field_count = read32();
                auto struct_type = std::make_shared<StructTypeDef>();
                struct_type->fields.reserve(field_count);
                for (uint32_t i = 0; i < field_count; ++i) {
                    std::string name = unit->constant_strings.at(read32());
                    bool has_constraint = read8() != 0;
                    std::shared_ptr<Closure> constraint;
                    if (has_constraint) {
                        constraint = captureChildClosure(read32());
                    }
                    bool has_default = read8() != 0;
                    std::shared_ptr<Closure> default_value;
                    if (has_default) {
                        default_value = captureChildClosure(read32());
                    }
                    struct_type->fields.push_back(StructFieldSpec{
                        std::move(name),
                        std::move(constraint),
                        std::move(default_value)
                    });
                }
                return Value::StructType(std::move(struct_type));
            }
            case Opcode::MakeEnumFamily: {
                auto family_def = std::make_shared<EnumFamilyDef>();
                family_def->node_id = read64();
                uint8_t count = read8();
                for (uint8_t i = 0; i < count; ++i) {
                    family_def->variants.push_back(unit->constant_strings.at(read32()));
                }
                return Value::EnumFamily(std::move(family_def));
            }
            case Opcode::MakeEnumeratedSet: {
                uint32_t element_count = read32();
                auto elements = std::make_shared<std::vector<Value>>();
                elements->reserve(element_count);
                for (uint32_t i = 0; i < element_count; ++i) {
                    elements->push_back(evalOperand());
                }
                return Value::EnumeratedSet(std::move(elements));
            }
            case Opcode::MakeArray: {
                uint32_t element_count = read32();
                auto elements = std::make_shared<std::vector<Value>>();
                elements->reserve(element_count);
                for (uint32_t i = 0; i < element_count; ++i) {
                    elements->push_back(evalOperand());
                }
                return Value::Array(std::move(elements));
            }
            case Opcode::MakeFString: {
                uint32_t part_count = read32();
                std::string res;
                for (uint32_t i = 0; i < part_count; ++i) {
                    Value part = evalOperand();
                    res += part.toString();
                }
                return Value(res);
            }
            case Opcode::MakeConstructedSet: {
                bool has_bound = read8() != 0;
                std::shared_ptr<Closure> bound;
                if (has_bound) {
                    bound = captureChildClosure(read32());
                }
                std::shared_ptr<Closure> predicate = captureChildClosure(read32());
                return Value::ConstructedSet(std::make_shared<ConstructedSetDef>(ConstructedSetDef{
                    std::move(bound),
                    std::move(predicate),
                }));
            }
            case Opcode::MakeSignature: {
                uint32_t parameter_count = read32();
                auto signature = std::make_shared<SignatureDef>();
                signature->parameters.reserve(parameter_count);
                for (uint32_t i = 0; i < parameter_count; ++i) {
                    std::string name = unit->constant_strings.at(read32());
                    bool has_constraint = read8() != 0;
                    std::shared_ptr<Closure> constraint;
                    if (has_constraint) {
                        constraint = captureChildClosure(read32());
                    }
                    signature->parameters.push_back(SignatureParameterSpec{
                        std::move(name),
                        std::move(constraint),
                    });
                }
                bool has_return_bound = read8() != 0;
                if (has_return_bound) {
                    signature->return_bound = captureChildClosure(read32());
                }
                return Value::Signature(std::move(signature));
            }
            case Opcode::MakeAnonStruct: {
                uint32_t field_count = read32();
                auto value = std::make_shared<std::unordered_map<std::string, Value>>();
                for (uint32_t i = 0; i < field_count; ++i) {
                    std::string name = unit->constant_strings.at(read32());
                    if (value->contains(name)) {
                        throw std::runtime_error("Duplicate field in anonymous struct literal: " + name);
                    }
                    (*value)[name] = evalOperand();
                }
                return Value::Struct(std::move(value));
            }
            case Opcode::ForEach: {
                Value iterable = evalOperand();
                uint32_t slot = read32();
                uint32_t body_len = read32();
                size_t body_start = pc;
                size_t body_end = body_start + body_len;
                try {
                    for (const auto& value : finiteElements(iterable)) {
                        assignLocal(slot, value);
                        evalOperandAt(body_start, body_end);
                    }
                } catch (BreakSignal& signal) {
                    dropLocal(slot);
                    pc = body_end;
                    return std::move(signal.value);
                } catch (...) {
                    dropLocal(slot);
                    throw;
                }
                dropLocal(slot);
                pc = body_end;
                return Value();
            }
            case Opcode::Let: {
                OperandType dest_type = static_cast<OperandType>(read8());

                switch (dest_type) {
                    case OperandType::StackLocal: {
                        uint32_t slot = read32();
                        Value value = evalOperand();
                        assignLocal(slot, value);
                        return value;
                    }
                    case OperandType::Identifier: {
                        std::string name = unit->constant_strings.at(read32());
                        Value value = evalOperand();
                        assignGlobal(name, value);
                        return value;
                    }
                    default:
                        throw std::runtime_error("Let instruction requires a local or global destination");
                }
            }
            case Opcode::Assign: {
                OperandType dest_type = static_cast<OperandType>(read8());

                switch (dest_type) {
                    case OperandType::StackLocal: {
                        uint32_t slot = read32();
                        Value value = evalOperand();
                        assignLocal(slot, value);
                        return value;
                    }
                    case OperandType::Capture: {
                        uint32_t slot = read32();
                        Value value = evalOperand();
                        assignCapture(slot, value);
                        return value;
                    }
                    case OperandType::Identifier: {
                        std::string name = unit->constant_strings.at(read32());
                        Value value = evalOperand();
                        assignGlobal(name, value);
                        return value;
                    }
                    default:
                        throw std::runtime_error("Assign instruction requires a local, capture, or global destination");
                }
            }
            case Opcode::DropBinding: {
                OperandType dest_type = static_cast<OperandType>(read8());
                switch (dest_type) {
                    case OperandType::StackLocal:
                        dropLocal(read32());
                        return Value();
                    case OperandType::Capture:
                        dropCapture(read32());
                        return Value();
                    case OperandType::Identifier:
                        dropGlobal(unit->constant_strings.at(read32()));
                        return Value();
                    default:
                        throw std::runtime_error("DropBinding instruction requires a local, capture, or global destination");
                }
            }
            case Opcode::StoreDeref: {
                Value pointer = evalOperand();
                Value value = evalOperand();
                return storeDereference(pointer, std::move(value));
            }
            case Opcode::EnforceConstraint: {
                Value constraint = evalOperand();
                Value value = evalOperand();
                return enforceConstraint(constraint, std::move(value), "local binding");
            }
            case Opcode::EnforceGlobalConstraint: {
                std::string name = unit->constant_strings.at(read32());
                Value value = evalOperand();
                auto found = globals.find("__chirp_constraint_" + name);
                if (found != globals.end() && found->second.type != ValueType::Null) {
                    return enforceConstraint(found->second, std::move(value), name);
                }
                return value;
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

Value Evaluator::evaluate(std::shared_ptr<ProgramUnit> unit,
                          std::unordered_map<std::string, Value>& globals,
                          std::ostream& out,
                          const std::unordered_map<std::string, Value>* registry,
                          const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls) {
    ExecutionState state(std::move(unit), globals, out, registry, trait_impls);
    return state.run();
}

Value Evaluator::construct_struct(std::shared_ptr<StructTypeDef> type,
                                  const std::vector<CallArgument>& args,
                                  std::unordered_map<std::string, Value>& globals,
                                  std::ostream& out,
                                  const std::unordered_map<std::string, Value>* registry,
                                  const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls) {
    auto empty_unit = std::make_shared<ProgramUnit>();
    ExecutionState state(std::move(empty_unit), globals, out, registry, trait_impls);
    return state.constructStruct(std::move(type), args);
}

Value Evaluator::invoke_value(const Value& callee,
                              const std::vector<CallArgument>& args,
                              std::unordered_map<std::string, Value>& globals,
                              std::ostream& out,
                              const std::unordered_map<std::string, Value>* registry,
                              const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls) {
    auto empty_unit = std::make_shared<ProgramUnit>();
    ExecutionState state(std::move(empty_unit), globals, out, registry, trait_impls);
    return state.invokeValue(callee, args);
}

} // namespace chirp::vm
