#include "chirp/vm.h"

#include "chirp/backend.h"
#include "chirp/frontend.h"
#include "compiler.h"
#include "evaluator.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace chirp::vm {

namespace {

int64_t require_int64(const BigInt& value, const std::string& message) {
    if (!value.fits_int64()) {
        throw std::runtime_error(message);
    }
    return value.to_int64();
}

std::string decode_string_literal(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        std::string decoded;
        decoded.reserve(value.size() - 2);
        for (size_t i = 1; i + 1 < value.size(); ++i) {
            char ch = value[i];
            if (ch == '\\' && i + 2 < value.size()) {
                char escaped = value[++i];
                switch (escaped) {
                    case 'n':
                        decoded.push_back('\n');
                        break;
                    case 'r':
                        decoded.push_back('\r');
                        break;
                    case 't':
                        decoded.push_back('\t');
                        break;
                    case '\\':
                        decoded.push_back('\\');
                        break;
                    case '"':
                        decoded.push_back('"');
                        break;
                    default:
                        decoded.push_back(escaped);
                        break;
                }
                continue;
            }
            decoded.push_back(ch);
        }
        return decoded;
    }
    return value;
}

std::string encode_string_literal(const std::string& value) {
    std::string encoded;
    encoded.reserve(value.size() + 2);
    encoded.push_back('"');
    for (char ch : value) {
        switch (ch) {
            case '\\':
                encoded += "\\\\";
                break;
            case '"':
                encoded += "\\\"";
                break;
            case '\n':
                encoded += "\\n";
                break;
            case '\r':
                encoded += "\\r";
                break;
            case '\t':
                encoded += "\\t";
                break;
            default:
                encoded.push_back(ch);
                break;
        }
    }
    encoded.push_back('"');
    return encoded;
}

std::string display_string(const Value& value) {
    if (value.type == ValueType::String) {
        return decode_string_literal(value.as_string);
    }
    return value.toString();
}

bool value_equals(const Value& left, const Value& right) {
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
        case ValueType::Struct:
            if (left.as_struct_instance_type != right.as_struct_instance_type ||
                left.as_struct->size() != right.as_struct->size()) {
                return false;
            }
            for (const auto& [name, value] : *left.as_struct) {
                auto it = right.as_struct->find(name);
                if (it == right.as_struct->end() || !value_equals(value, it->second)) {
                    return false;
                }
            }
            return true;
        case ValueType::Array:
            if (left.as_array->size() != right.as_array->size()) {
                return false;
            }
            for (size_t i = 0; i < left.as_array->size(); ++i) {
                if (!value_equals(left.as_array->at(i), right.as_array->at(i))) {
                    return false;
                }
            }
            return true;
        case ValueType::EnumeratedSet:
            if (left.as_set_elements->size() != right.as_set_elements->size()) {
                return false;
            }
            for (size_t i = 0; i < left.as_set_elements->size(); ++i) {
                if (!value_equals(left.as_set_elements->at(i), right.as_set_elements->at(i))) {
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
            return left.as_enum_family == right.as_enum_family;
        case ValueType::EnumVariant:
            return left.as_enum_variant == right.as_enum_variant;
        case ValueType::Range:
            return value_equals(*left.as_range->start, *right.as_range->start) &&
                   value_equals(*left.as_range->end, *right.as_range->end) &&
                   left.as_range->inclusive_end == right.as_range->inclusive_end;
    }
    return false;
}

void append_unique(std::vector<Value>& values, const Value& candidate) {
    auto found = std::find_if(values.begin(), values.end(), [&](const Value& existing) {
        return value_equals(existing, candidate);
    });
    if (found == values.end()) {
        values.push_back(candidate);
    }
}

std::string pointer_key(const void* ptr) {
    std::ostringstream oss;
    oss << ptr;
    return oss.str();
}

std::string type_key(const Value& type) {
    switch (type.type) {
        case ValueType::TypeValue:
            return "type:" + pointer_key(type.as_type_value.get());
        case ValueType::StructType:
            return "struct:" + pointer_key(type.as_struct_type.get());
        default:
            throw std::runtime_error("Expected a type value");
    }
}

Value type_of_value(const Value& value, const std::unordered_map<std::string, Value>& globals) {
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

std::vector<Value> finite_elements(const Value& set) {
    if (set.type == ValueType::EnumeratedSet) {
        return *set.as_set_elements;
    }
    if (set.type == ValueType::Array) {
        return *set.as_array;
    }
    if (set.type == ValueType::TypeValue &&
        set.as_type_value &&
        set.as_type_value->kind == TypeValueDef::Kind::Finite) {
        std::vector<Value> values;
        for (uint64_t i = 0; i < set.as_type_value->finite_count; ++i) {
            values.push_back(Value::Minted(set.as_type_value, i));
        }
        return values;
    }
    if (set.type == ValueType::CompositeSet) {
        std::vector<Value> left = finite_elements(*set.as_composite_set->left);
        std::vector<Value> right = finite_elements(*set.as_composite_set->right);
        if (set.as_composite_set->op == CompositeSetDef::Op::Union) {
            for (const auto& value : right) {
                append_unique(left, value);
            }
            return left;
        }

        std::vector<Value> result;
        for (const auto& value : left) {
            auto found = std::find_if(right.begin(), right.end(), [&](const Value& other) {
                return value_equals(value, other);
            });
            if (found != right.end()) {
                append_unique(result, value);
            }
        }
        return result;
    }
    throw std::runtime_error("Set is not finitely enumerable");
}

void drop_value_vm(const Value& value,
                   std::unordered_map<std::string, Value>& globals,
                   std::ostream& out,
                   const std::unordered_map<std::string, Value>* registry,
                   const std::unordered_map<uint64_t, std::unordered_map<std::string, Value>>* trait_impls) {
    if (registry == nullptr || trait_impls == nullptr) {
        return;
    }
    auto trait_it = registry->find("traits.drop");
    if (trait_it == registry->end()) {
        return;
    }
    Value subject_type = type_of_value(value, globals);
    if (trait_it->second.type != ValueType::Trait ||
        (subject_type.type != ValueType::TypeValue && subject_type.type != ValueType::StructType)) {
        return;
    }
    auto impls_for_trait = trait_impls->find(trait_it->second.as_trait->id);
    if (impls_for_trait == trait_impls->end()) {
        return;
    }
    auto impl_it = impls_for_trait->second.find(type_key(subject_type));
    if (impl_it == impls_for_trait->second.end()) {
        return;
    }
    if (impl_it->second.type != ValueType::Struct || impl_it->second.as_struct == nullptr) {
        throw std::runtime_error("Drop implementation must be a struct instance");
    }
    auto drop_fn = impl_it->second.as_struct->find("drop");
    if (drop_fn == impl_it->second.as_struct->end()) {
        throw std::runtime_error("Drop implementation missing drop");
    }
    Evaluator evaluator;
    Value result = evaluator.invoke_value(
        drop_fn->second,
        {CallArgument{std::nullopt, value}},
        globals,
        out,
        registry,
        trait_impls);
    if (result.type != ValueType::Null) {
        throw std::runtime_error("Drop implementation must return void");
    }
}

} // namespace

class VmSession : public backend::Session {
    std::ostream& out_;
    backend::SessionExpectations expectations_;
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, Value> registry_;
    std::unordered_map<uint64_t, std::unordered_map<std::string, Value>> trait_impls_;
    bool testing_enabled_ = false;
    bool stdin_injected_ = false;
    std::string injected_stdin_;
    size_t stdin_cursor_ = 0;
    uint64_t next_type_id_ = 1;
    uint64_t next_trait_id_ = 1;
    uint64_t next_heap_id_ = 1;
    std::unordered_set<std::string> global_final_bindings_;

public:
    VmSession(std::ostream& out, bool testing_enabled) : out_(out), testing_enabled_(testing_enabled) {
        globals_["int"] = create_type("int", TypeValueDef::Kind::Primitive);
        globals_["bool"] = create_type("bool", TypeValueDef::Kind::Primitive);
        globals_["string"] = create_type("string", TypeValueDef::Kind::Primitive);
        globals_["char"] = create_type("char", TypeValueDef::Kind::Primitive);
        globals_["symbol"] = create_type("symbol", TypeValueDef::Kind::Primitive);
        globals_["EnumVariant"] = create_type("EnumVariant", TypeValueDef::Kind::Primitive);
        globals_["__void_type"] = create_type("void", TypeValueDef::Kind::Primitive);
        globals_["any"] = create_type("any", TypeValueDef::Kind::Primitive);
        globals_["type"] = create_type("type", TypeValueDef::Kind::Meta);
        globals_["lambda"] = create_type("lambda", TypeValueDef::Kind::Lambda);
        globals_["trait"] = create_type("trait", TypeValueDef::Kind::Trait);
        globals_["__heap_allocation_type"] = create_type("heap_allocation", TypeValueDef::Kind::Primitive);
        globals_["__heap_shared_allocation_type"] = create_type("heap_shared_allocation", TypeValueDef::Kind::Primitive);
        globals_["true"] = Value(true);
        globals_["false"] = Value(false);
        globals_["`import"] = Value(NativeFunc(make_import_fn()));
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) override {
        Compiler compiler(&global_final_bindings_);
        auto unit = compiler.compile(stmts);

        Evaluator evaluator;
        Value result = evaluator.evaluate(unit, globals_, out_, &registry_, &trait_impls_);
        if (result.type != ValueType::Null) {
            out_ << result.toString() << "\n";
        }
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts, std::string label) override {
        execute(stmts);
    }

    void execute_source(std::string source, std::string label) override {
        auto tokens = frontend::tokenize(source);
        auto stmts = frontend::parse(tokens);
        execute(stmts, label);
    }

    void execute_boot_source(std::string source, std::string label) override {
        auto tokens = frontend::tokenize(source);
        auto stmts = frontend::parse(tokens);
        Compiler compiler(&global_final_bindings_);
        auto unit = compiler.compile(stmts);
        Evaluator evaluator;
        evaluator.evaluate(unit, globals_, out_, &registry_, &trait_impls_);
    }

    void set_chirp_root(std::string path) override {
    }

    backend::SessionExpectations getExpectations() const override {
        return expectations_;
    }

private:
    Value create_type(const std::string& name, TypeValueDef::Kind kind, uint64_t finite_count = 0) {
        auto def = std::make_shared<TypeValueDef>();
        def->kind = kind;
        def->name = name;
        def->id = next_type_id_++;
        def->finite_count = finite_count;
        return Value::Type(std::move(def));
    }

    std::vector<CallArgument> require_struct_bundle(const Value& bundle, const std::string& label) const {
        if (bundle.type != ValueType::Struct) {
            throw std::runtime_error("`" + label + " expects a struct argument bundle");
        }
        std::vector<CallArgument> args;
        args.reserve(bundle.as_struct->size());
        for (const auto& [name, value] : *bundle.as_struct) {
            args.push_back(CallArgument{std::optional<std::string>(name), value});
        }
        return args;
    }

    NativeFunc make_import_fn() {
        return [this](const std::vector<CallArgument>& args) -> Value {
            if (args.empty() || args[0].name.has_value() || args[0].value.type != ValueType::String) {
                throw std::runtime_error("`import expects a positional string key");
            }

            const std::string& key = args[0].value.as_string;

            if (key == "io.print") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& print_args) -> Value {
                    if (print_args.size() != 1 || print_args[0].name.has_value()) {
                        throw std::runtime_error("`print expects one positional argument");
                    }
                    out_ << display_string(print_args[0].value);
                    out_ << "\n";
                    return Value();
                }));
            }

            if (key == "io.write") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& write_args) -> Value {
                    if (write_args.size() != 2 || write_args[0].name.has_value() || write_args[1].name.has_value()) {
                        throw std::runtime_error("`write expects two positional arguments");
                    }
                    if (write_args[1].value.type != ValueType::Int) {
                        throw std::runtime_error("`write expects 'to' to be an integer file descriptor");
                    }

                    if (write_args[1].value.as_int == BigInt(1)) {
                        out_ << display_string(write_args[0].value);
                    } else if (write_args[1].value.as_int == BigInt(2)) {
                        std::cerr << display_string(write_args[0].value);
                    } else {
                        throw std::runtime_error("Unsupported file descriptor for `write");
                    }
                    return Value();
                }));
            }

            if (key == "io.input") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& input_args) -> Value {
                    if (!input_args.empty()) {
                        throw std::runtime_error("`input expects zero arguments");
                    }

                    std::string line;
                    if (stdin_injected_) {
                        if (stdin_cursor_ < injected_stdin_.size()) {
                            size_t found = injected_stdin_.find('\n', stdin_cursor_);
                            if (found != std::string::npos) {
                                line = injected_stdin_.substr(stdin_cursor_, found - stdin_cursor_);
                                stdin_cursor_ = found + 1;
                            } else {
                                line = injected_stdin_.substr(stdin_cursor_);
                                stdin_cursor_ = injected_stdin_.size();
                            }
                        }
                    } else if (!std::getline(std::cin, line)) {
                        line.clear();
                    }

                    return Value(encode_string_literal(line));
                }));
            }

            if (key == "system.register") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& register_args) -> Value {
                    if (register_args.size() != 2 || register_args[0].name.has_value() || register_args[1].name.has_value()) {
                        throw std::runtime_error("`register expects two positional arguments");
                    }
                    if (register_args[0].value.type != ValueType::String) {
                        throw std::runtime_error("`register key must be a string");
                    }
                    registry_[decode_string_literal(register_args[0].value.as_string)] = register_args[1].value;
                    return Value();
                }));
            }

            if (key == "values.same") {
                return Value(NativeFunc([](const std::vector<CallArgument>& same_args) -> Value {
                    if (same_args.size() != 2 || same_args[0].name.has_value() || same_args[1].name.has_value()) {
                        throw std::runtime_error("`same expects two positional arguments");
                    }
                    return Value(value_equals(same_args[0].value, same_args[1].value));
                }));
            }

            if (key == "types.type_of") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& type_args) -> Value {
                    if (type_args.size() != 1 || type_args[0].name.has_value()) {
                        throw std::runtime_error("`type_of expects one positional argument");
                    }
                    return type_of_value(type_args[0].value, globals_);
                }));
            }

            if (key == "types.is_struct_type") {
                return Value(NativeFunc([](const std::vector<CallArgument>& type_args) -> Value {
                    if (type_args.size() != 1 || type_args[0].name.has_value()) {
                        throw std::runtime_error("`is_struct_type expects one positional argument");
                    }
                    return Value(type_args[0].value.type == ValueType::StructType);
                }));
            }

            if (key == "types.construction_args") {
                return Value(NativeFunc([](const std::vector<CallArgument>& type_args) -> Value {
                    if (type_args.size() != 1 || type_args[0].name.has_value()) {
                        throw std::runtime_error("`construction_args expects one positional argument");
                    }
                    if (type_args[0].value.type != ValueType::StructType) {
                        throw std::runtime_error("`construction_args expects a struct type");
                    }
                    return Value::StructType(type_args[0].value.as_struct_type);
                }));
            }

            if (key == "types.construct") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& type_args) -> Value {
                    if (type_args.size() != 2 || type_args[0].name.has_value() || type_args[1].name.has_value()) {
                        throw std::runtime_error("`construct expects two positional arguments");
                    }
                    if (type_args[0].value.type != ValueType::StructType) {
                        throw std::runtime_error("`construct expects a struct type");
                    }
                    Evaluator evaluator;
                    return evaluator.construct_struct(
                        type_args[0].value.as_struct_type,
                        require_struct_bundle(type_args[1].value, "construct"),
                        globals_,
                        out_,
                        &registry_,
                        &trait_impls_);
                }));
            }

            if (key == "types.mint_finite" || key == "types.mint_infinite" || key == "types.mint_host") {
                return Value(NativeFunc([this, key](const std::vector<CallArgument>& mint_args) -> Value {
                    if (mint_args.empty() || mint_args[0].name.has_value() || mint_args[0].value.type != ValueType::Int) {
                        throw std::runtime_error("`mint_* expects one positional integer argument");
                    }
                    int64_t count = require_int64(mint_args[0].value.as_int, "`mint_* count is out of range");
                    if (count < 0) {
                        throw std::runtime_error("`mint_* count must be non-negative");
                    }

                    uint64_t finite_count = key == "types.mint_finite" ? static_cast<uint64_t>(count) : 0;
                    Value type = create_type("minted_type_" + std::to_string(next_type_id_), TypeValueDef::Kind::Finite, finite_count);
                    auto values_array = std::make_shared<std::vector<Value>>();
                    for (uint64_t i = 0; i < finite_count; ++i) {
                        values_array->push_back(Value::Minted(type.as_type_value, i));
                    }
                    auto result_struct = std::make_shared<std::unordered_map<std::string, Value>>();
                    (*result_struct)["type"] = type;
                    (*result_struct)["values"] = Value::Array(values_array);
                    return Value::Struct(result_struct);
                }));
            }

            if (key == "memory.heap_allocation") {
                return globals_.at("__heap_allocation_type");
            }

            if (key == "system.exit") {
                return Value(NativeFunc([](const std::vector<CallArgument>& exit_args) -> Value {
                    if (exit_args.size() != 1 || exit_args[0].name.has_value()) {
                        throw std::runtime_error("`exit expects one positional argument");
                    }
                    if (exit_args[0].value.type != ValueType::Int ||
                        exit_args[0].value.as_int < BigInt(0) ||
                        exit_args[0].value.as_int > BigInt(255)) {
                        throw std::runtime_error("`exit expects an integer exit code between 0 and 255");
                    }
                    throw backend::ScriptExit(static_cast<int>(exit_args[0].value.as_int.to_int64()));
                }));
            }

            if (key == "memory.heap_shared_allocation") {
                return globals_.at("__heap_shared_allocation_type");
            }

            if (key == "memory.heap_create") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& heap_args) -> Value {
                    if (heap_args.size() != 1 || heap_args[0].name.has_value()) {
                        throw std::runtime_error("`heap_create expects one positional argument");
                    }
                    auto state = std::make_shared<HeapState>();
                    state->id = next_heap_id_++;
                    state->stored = heap_args[0].value;
                    state->shared = false;
                    return Value::Heap(std::move(state), globals_.at("__heap_allocation_type").as_type_value);
                }));
            }

            if (key == "memory.heap_destroy") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& heap_args) -> Value {
                    if (heap_args.size() != 1 || heap_args[0].name.has_value()) {
                        throw std::runtime_error("`heap_destroy expects one positional argument");
                    }
                    if (heap_args[0].value.type != ValueType::Heap || heap_args[0].value.as_heap == nullptr) {
                        throw std::runtime_error("`heap_destroy expects a heap allocation");
                    }
                    if (heap_args[0].value.as_heap->destroyed) {
                        return Value();
                    }
                    Value stored = heap_args[0].value.as_heap->stored;
                    heap_args[0].value.as_heap->destroyed = true;
                    drop_value_vm(stored, globals_, out_, &registry_, &trait_impls_);
                    return Value();
                }));
            }

            if (key == "memory.heap_shared_create") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& heap_args) -> Value {
                    if (heap_args.size() != 1 || heap_args[0].name.has_value()) {
                        throw std::runtime_error("`heap_shared_create expects one positional argument");
                    }
                    auto state = std::make_shared<HeapState>();
                    state->id = next_heap_id_++;
                    state->stored = heap_args[0].value;
                    state->shared = true;
                    // Fresh shared allocations are produced as unbound owning temporaries.
                    // The first binding/capture store retains them up to 1.
                    state->strong_count = 0;
                    return Value::Heap(std::move(state), globals_.at("__heap_shared_allocation_type").as_type_value);
                }));
            }

            if (key == "memory.heap_shared_destroy") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& heap_args) -> Value {
                    if (heap_args.size() != 1 || heap_args[0].name.has_value()) {
                        throw std::runtime_error("`heap_shared_destroy expects one positional argument");
                    }
                    if (heap_args[0].value.type != ValueType::Heap || heap_args[0].value.as_heap == nullptr) {
                        throw std::runtime_error("`heap_shared_destroy expects a shared heap allocation");
                    }
                    if (heap_args[0].value.as_heap->strong_count > 0) {
                        --heap_args[0].value.as_heap->strong_count;
                    }
                    if (heap_args[0].value.as_heap->strong_count == 0) {
                        Value stored = heap_args[0].value.as_heap->stored;
                        heap_args[0].value.as_heap->destroyed = true;
                        drop_value_vm(stored, globals_, out_, &registry_, &trait_impls_);
                    }
                    return Value();
                }));
            }

            if (key == "traits.make") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& trait_args) -> Value {
                    if (trait_args.size() != 1 || trait_args[0].name.has_value()) {
                        throw std::runtime_error("`make_trait expects one positional argument");
                    }
                    auto trait = std::make_shared<TraitDef>();
                    trait->id = next_trait_id_++;
                    trait->interface = trait_args[0].value;
                    return Value::Trait(std::move(trait));
                }));
            }

            if (key == "traits.interface") {
                return Value(NativeFunc([](const std::vector<CallArgument>& trait_args) -> Value {
                    if (trait_args.size() != 1 || trait_args[0].name.has_value() || trait_args[0].value.type != ValueType::Trait) {
                        throw std::runtime_error("`interface_of expects one trait argument");
                    }
                    return trait_args[0].value.as_trait->interface;
                }));
            }

	            if (key == "traits.implement") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& impl_args) -> Value {
                    if (impl_args.size() != 3) {
                        throw std::runtime_error("`implement expects three named arguments");
                    }

                    const Value* trait = nullptr;
                    const Value* on = nullptr;
                    const Value* impl = nullptr;
                    for (const auto& arg : impl_args) {
                        if (!arg.name.has_value()) {
                            throw std::runtime_error("`implement expects named arguments");
                        }
                        if (*arg.name == "trait") trait = &arg.value;
                        else if (*arg.name == "on") on = &arg.value;
                        else if (*arg.name == "impl") impl = &arg.value;
                    }

                    if (trait == nullptr || on == nullptr || impl == nullptr || trait->type != ValueType::Trait) {
                        throw std::runtime_error("`implement expects trait, on, and impl arguments");
                    }

                    std::string on_key = type_key(*on);
                    auto& impls_for_trait = trait_impls_[trait->as_trait->id];
                    if (impls_for_trait.contains(on_key)) {
                        throw std::runtime_error("Duplicate implementation for trait/on pair");
                    }

                    Value interface = trait->as_trait->interface;
                    Value validated_impl = *impl;
                    if (interface.type == ValueType::Null) {
                        if (validated_impl.type != ValueType::Null) {
                            throw std::runtime_error("Marker trait implementations must be void");
                        }
                    } else if (interface.type == ValueType::StructType) {
                        Evaluator evaluator;
                        validated_impl = evaluator.construct_struct(
                            interface.as_struct_type,
                            require_struct_bundle(validated_impl, "implement"),
                            globals_,
                            out_,
                            &registry_,
                            &trait_impls_);
                    } else {
                        throw std::runtime_error("`implement trait interface must be either void or a struct type");
                    }

                    impls_for_trait.emplace(std::move(on_key), std::move(validated_impl));
                    return Value();
                }));
            }

            if (key == "traits.implements") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& impl_args) -> Value {
                    if (impl_args.size() != 2 || impl_args[0].name.has_value() || impl_args[1].name.has_value()) {
                        throw std::runtime_error("`implements expects two positional arguments");
                    }
                    if (impl_args[0].value.type != ValueType::Trait) {
                        throw std::runtime_error("`implements expects a trait as first argument");
                    }
                    auto trait_it = trait_impls_.find(impl_args[0].value.as_trait->id);
                    if (trait_it == trait_impls_.end()) {
                        return Value(false);
                    }
                    return Value(trait_it->second.contains(type_key(impl_args[1].value)));
                }));
            }

            if (key == "traits.implementation") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& impl_args) -> Value {
                    if (impl_args.size() != 2 || impl_args[0].name.has_value() || impl_args[1].name.has_value()) {
                        throw std::runtime_error("`implementation expects two positional arguments");
                    }
                    if (impl_args[0].value.type != ValueType::Trait) {
                        throw std::runtime_error("`implementation expects a trait as first argument");
                    }
                    auto trait_it = trait_impls_.find(impl_args[0].value.as_trait->id);
                    if (trait_it == trait_impls_.end()) {
                        throw std::runtime_error("No implementation registered for trait");
                    }
                    auto impl_it = trait_it->second.find(type_key(impl_args[1].value));
                    if (impl_it == trait_it->second.end()) {
                        throw std::runtime_error("No implementation registered for type");
                    }
                    return impl_it->second;
                }));
            }

            if (key == "compute.invoke") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& invoke_args) -> Value {
                    if (invoke_args.size() != 2 || invoke_args[0].name.has_value() || invoke_args[1].name.has_value()) {
                        throw std::runtime_error("`invoke expects two positional arguments");
                    }
                    Evaluator evaluator;
                    return evaluator.invoke_value(
                        invoke_args[0].value,
                        require_struct_bundle(invoke_args[1].value, "invoke"),
                        globals_,
                        out_,
                        &registry_,
                        &trait_impls_);
                }));
            }

            if (key == "compute.is_pure") {
                return Value(NativeFunc([](const std::vector<CallArgument>& purity_args) -> Value {
                    if (purity_args.size() != 1 || purity_args[0].name.has_value()) {
                        throw std::runtime_error("`is_pure expects one positional argument");
                    }
                    return Value(
                        purity_args[0].value.type == ValueType::Closure ||
                        purity_args[0].value.type == ValueType::NativeFunc);
                }));
            }

            if (key == "compute.lambda_param_space") {
                return Value(NativeFunc([](const std::vector<CallArgument>& lambda_args) -> Value {
                    if (lambda_args.size() != 1 || lambda_args[0].name.has_value()) {
                        throw std::runtime_error("`lambda_param_space expects one positional argument");
                    }
                    if (lambda_args[0].value.type != ValueType::Closure) {
                        throw std::runtime_error("`lambda_param_space expects a lambda");
                    }
                    auto type = std::make_shared<StructTypeDef>();
                    for (const auto& name : lambda_args[0].value.as_closure->unit->parameter_names) {
                        type->fields.push_back(StructFieldSpec{name, nullptr, nullptr});
                    }
                    return Value::StructType(std::move(type));
                }));
            }

            if (key == "compute.lambda_result_space") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& lambda_args) -> Value {
                    if (lambda_args.size() != 2 || lambda_args[0].name.has_value() || lambda_args[1].name.has_value()) {
                        throw std::runtime_error("`lambda_result_space expects two positional arguments");
                    }
                    return globals_.at("any");
                }));
            }

            if (key == "sets.types_in_set") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& set_args) -> Value {
                    if (set_args.size() != 1 || set_args[0].name.has_value()) {
                        throw std::runtime_error("`types_in_set expects one positional argument");
                    }
                    auto elements = std::make_shared<std::vector<Value>>();
                    for (const auto& element : finite_elements(set_args[0].value)) {
                        append_unique(*elements, type_of_value(element, globals_));
                    }
                    return Value::EnumeratedSet(std::move(elements));
                }));
            }

            if (key == "sets.enumerable") {
                return Value(NativeFunc([](const std::vector<CallArgument>& set_args) -> Value {
                    if (set_args.size() != 1 || set_args[0].name.has_value()) {
                        throw std::runtime_error("`is_enumerable expects one positional argument");
                    }
                    try {
                        (void)finite_elements(set_args[0].value);
                        return Value(true);
                    } catch (...) {
                        return Value(false);
                    }
                }));
            }

            if (key == "sets.coextensive") {
                return Value(NativeFunc([](const std::vector<CallArgument>& set_args) -> Value {
                    if (set_args.size() != 2 || set_args[0].name.has_value() || set_args[1].name.has_value()) {
                        throw std::runtime_error("`coextensive expects two positional arguments");
                    }
                    std::vector<Value> left = finite_elements(set_args[0].value);
                    std::vector<Value> right = finite_elements(set_args[1].value);
                    if (left.size() != right.size()) {
                        return Value(false);
                    }
                    for (const auto& value : left) {
                        auto found = std::find_if(right.begin(), right.end(), [&](const Value& other) {
                            return value_equals(value, other);
                        });
                        if (found == right.end()) {
                            return Value(false);
                        }
                    }
                    return Value(true);
                }));
            }

            if (key == "testing.enabled") {
                return Value(testing_enabled_);
            }

            if (key == "testing.inject_stdin") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& inject_args) -> Value {
                    if (inject_args.size() != 1 || inject_args[0].name.has_value()) {
                        throw std::runtime_error("`inject_stdin expects one positional argument");
                    }
                    if (inject_args[0].value.type != ValueType::String) {
                        throw std::runtime_error("`inject_stdin expects a string");
                    }
                    stdin_injected_ = true;
                    injected_stdin_ += decode_string_literal(inject_args[0].value.as_string);
                    return Value();
                }));
            }

            if (key == "testing.expect") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& expect_args) -> Value {
                    if (expect_args.size() != 1 || expect_args[0].name.has_value()) {
                        throw std::runtime_error("`expect expects one positional argument");
                    }
                    expectations_.has_expectations = true;
                    expectations_.expectation_checks += 1;
                    if (expect_args[0].value.type != ValueType::Bool) {
                        throw std::runtime_error("`expect expects a Bool expression");
                    }
                    if (expect_args[0].value.as_int == BigInt(0)) {
                        throw std::runtime_error("`expect check failed");
                    }
                    return Value();
                }));
            }

            if (key == "testing.expect_stdout") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& expect_args) -> Value {
                    if (expect_args.size() != 1 || expect_args[0].name.has_value()) {
                        throw std::runtime_error("`expect_stdout expects one positional argument");
                    }
                    if (expect_args[0].value.type != ValueType::String) {
                        throw std::runtime_error("`expect_stdout expects a string");
                    }
                    expectations_.has_expectations = true;
                    std::string expected = decode_string_literal(expect_args[0].value.as_string);
                    if (!expectations_.expected_stdout.has_value()) {
                        expectations_.expected_stdout = std::move(expected);
                    } else {
                        *expectations_.expected_stdout += expected;
                    }
                    return Value();
                }));
            }

            if (key == "testing.expect_stderr") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& expect_args) -> Value {
                    if (expect_args.size() != 1 || expect_args[0].name.has_value()) {
                        throw std::runtime_error("`expect_stderr expects one positional argument");
                    }
                    if (expect_args[0].value.type != ValueType::String) {
                        throw std::runtime_error("`expect_stderr expects a string");
                    }
                    expectations_.has_expectations = true;
                    std::string expected = decode_string_literal(expect_args[0].value.as_string);
                    if (!expectations_.expected_stderr.has_value()) {
                        expectations_.expected_stderr = std::move(expected);
                    } else {
                        *expectations_.expected_stderr += expected;
                    }
                    return Value();
                }));
            }

            if (key == "testing.expect_exit") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& expect_args) -> Value {
                    if (expect_args.size() != 1 || expect_args[0].name.has_value()) {
                        throw std::runtime_error("`expect_exit expects one positional argument");
                    }
                    if (expect_args[0].value.type != ValueType::Int ||
                        expect_args[0].value.as_int < BigInt(0) ||
                        expect_args[0].value.as_int > BigInt(255)) {
                        throw std::runtime_error("`expect_exit expects an integer exit code between 0 and 255");
                    }
                    expectations_.has_expectations = true;
                    expectations_.expected_exit = static_cast<int>(expect_args[0].value.as_int.to_int64());
                    return Value();
                }));
            }

            if (key == "testing.expect_test_failure") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& expect_args) -> Value {
                    if (!expect_args.empty()) {
                        throw std::runtime_error("`expect_test_failure expects no arguments");
                    }
                    expectations_.has_expectations = true;
                    expectations_.expect_test_failure = true;
                    return Value();
                }));
            }

            throw std::runtime_error("Unsupported native import in VM: " + key);
        };
    }
};

std::unique_ptr<chirp::backend::Session> createSession(std::ostream& out, bool testing_enabled) {
    return std::make_unique<VmSession>(out, testing_enabled);
}

} // namespace chirp::vm
