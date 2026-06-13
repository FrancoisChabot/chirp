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

} // namespace

class VmSession : public backend::Session {
    std::ostream& out_;
    backend::SessionExpectations expectations_;
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<uint64_t, std::unordered_map<std::string, Value>> trait_impls_;
    uint64_t next_type_id_ = 1;
    uint64_t next_trait_id_ = 1;

public:
    VmSession(std::ostream& out) : out_(out) {
        globals_["int"] = create_type("int", TypeValueDef::Kind::Primitive);
        globals_["bool"] = create_type("bool", TypeValueDef::Kind::Primitive);
        globals_["string"] = create_type("string", TypeValueDef::Kind::Primitive);
        globals_["char"] = create_type("char", TypeValueDef::Kind::Primitive);
        globals_["symbol"] = create_type("symbol", TypeValueDef::Kind::Primitive);
        globals_["any"] = create_type("any", TypeValueDef::Kind::Primitive);
        globals_["type"] = create_type("type", TypeValueDef::Kind::Meta);
        globals_["lambda"] = create_type("lambda", TypeValueDef::Kind::Lambda);
        globals_["trait"] = create_type("trait", TypeValueDef::Kind::Trait);
        globals_["true"] = Value(true);
        globals_["false"] = Value(false);
        globals_["`import"] = Value(NativeFunc(make_import_fn()));
    }

    void execute(const std::vector<std::unique_ptr<frontend::Stmt>>& stmts) override {
        Compiler compiler;
        auto unit = compiler.compile(stmts);

        Evaluator evaluator;
        Value result = evaluator.evaluate(unit, globals_, out_);
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
        Compiler compiler;
        auto unit = compiler.compile(stmts);
        Evaluator evaluator;
        evaluator.evaluate(unit, globals_, out_);
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

            if (key == "\"io.print\"") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& print_args) -> Value {
                    for (size_t i = 0; i < print_args.size(); ++i) {
                        if (print_args[i].name.has_value()) {
                            throw std::runtime_error("`print does not support named arguments");
                        }
                        if (i > 0) out_ << " ";
                        out_ << print_args[i].value.toString();
                    }
                    out_ << "\n";
                    return Value();
                }));
            }

            if (key == "\"system.register\"") {
                return Value(NativeFunc([](const std::vector<CallArgument>&) -> Value {
                    return Value();
                }));
            }

            if (key == "\"values.same\"") {
                return Value(NativeFunc([](const std::vector<CallArgument>& same_args) -> Value {
                    if (same_args.size() != 2 || same_args[0].name.has_value() || same_args[1].name.has_value()) {
                        throw std::runtime_error("`same expects two positional arguments");
                    }
                    return Value(value_equals(same_args[0].value, same_args[1].value));
                }));
            }

            if (key == "\"types.type_of\"") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& type_args) -> Value {
                    if (type_args.size() != 1 || type_args[0].name.has_value()) {
                        throw std::runtime_error("`type_of expects one positional argument");
                    }
                    return type_of_value(type_args[0].value, globals_);
                }));
            }

            if (key == "\"types.is_struct_type\"") {
                return Value(NativeFunc([](const std::vector<CallArgument>& type_args) -> Value {
                    if (type_args.size() != 1 || type_args[0].name.has_value()) {
                        throw std::runtime_error("`is_struct_type expects one positional argument");
                    }
                    return Value(type_args[0].value.type == ValueType::StructType);
                }));
            }

            if (key == "\"types.construction_args\"") {
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

            if (key == "\"types.construct\"") {
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
                        out_);
                }));
            }

            if (key == "\"types.mint_finite\"" || key == "\"types.mint_infinite\"" || key == "\"types.mint_host\"") {
                return Value(NativeFunc([this, key](const std::vector<CallArgument>& mint_args) -> Value {
                    if (mint_args.empty() || mint_args[0].name.has_value() || mint_args[0].value.type != ValueType::Int) {
                        throw std::runtime_error("`mint_* expects one positional integer argument");
                    }
                    int64_t count = mint_args[0].value.as_int;
                    if (count < 0) {
                        throw std::runtime_error("`mint_* count must be non-negative");
                    }

                    uint64_t finite_count = key == "\"types.mint_finite\"" ? static_cast<uint64_t>(count) : 0;
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

            if (key == "\"traits.make\"") {
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

            if (key == "\"traits.interface\"") {
                return Value(NativeFunc([](const std::vector<CallArgument>& trait_args) -> Value {
                    if (trait_args.size() != 1 || trait_args[0].name.has_value() || trait_args[0].value.type != ValueType::Trait) {
                        throw std::runtime_error("`interface_of expects one trait argument");
                    }
                    return trait_args[0].value.as_trait->interface;
                }));
            }

            if (key == "\"traits.implement\"") {
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

                    trait_impls_[trait->as_trait->id][type_key(*on)] = *impl;
                    return Value();
                }));
            }

            if (key == "\"traits.implements\"") {
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

            if (key == "\"traits.implementation\"") {
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

            if (key == "\"compute.invoke\"") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& invoke_args) -> Value {
                    if (invoke_args.size() != 2 || invoke_args[0].name.has_value() || invoke_args[1].name.has_value()) {
                        throw std::runtime_error("`invoke expects two positional arguments");
                    }
                    Evaluator evaluator;
                    return evaluator.invoke_value(
                        invoke_args[0].value,
                        require_struct_bundle(invoke_args[1].value, "invoke"),
                        globals_,
                        out_);
                }));
            }

            if (key == "\"compute.is_pure\"") {
                return Value(NativeFunc([](const std::vector<CallArgument>& purity_args) -> Value {
                    if (purity_args.size() != 1 || purity_args[0].name.has_value()) {
                        throw std::runtime_error("`is_pure expects one positional argument");
                    }
                    return Value(
                        purity_args[0].value.type == ValueType::Closure ||
                        purity_args[0].value.type == ValueType::NativeFunc);
                }));
            }

            if (key == "\"compute.lambda_param_space\"") {
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

            if (key == "\"compute.lambda_result_space\"") {
                return Value(NativeFunc([this](const std::vector<CallArgument>& lambda_args) -> Value {
                    if (lambda_args.size() != 2 || lambda_args[0].name.has_value() || lambda_args[1].name.has_value()) {
                        throw std::runtime_error("`lambda_result_space expects two positional arguments");
                    }
                    return globals_.at("any");
                }));
            }

            if (key == "\"sets.types_in_set\"") {
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

            if (key == "\"sets.enumerable\"") {
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

            if (key == "\"sets.coextensive\"") {
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

            throw std::runtime_error("Unsupported native import in VM");
        };
    }
};

std::unique_ptr<chirp::backend::Session> createSession(std::ostream& out, bool testing_enabled) {
    return std::make_unique<VmSession>(out);
}

} // namespace chirp::vm
