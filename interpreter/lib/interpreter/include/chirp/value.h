#pragma once

#include <memory>
#include <variant>
#include <string>
#include <string_view>
#include <vector>

namespace chirp::frontend {
class LambdaExpr;
}

namespace chirp::interpreter {

class Type;
class Binding;

class Value {
public:
    struct TypeTag {
        std::shared_ptr<const Type> t;
        bool operator==(const TypeTag& other) const { return t == other.t; }
    };
    struct BindingTag {
        std::shared_ptr<Binding> b;
        bool operator==(const BindingTag& other) const { return b == other.b; }
    };
    struct EnumeratedSetTag {
        std::shared_ptr<std::vector<Value>> elements;
        bool operator==(const EnumeratedSetTag& other) const;
    };
    struct LambdaTag {
        const frontend::LambdaExpr* lambda;
        bool operator==(const LambdaTag& other) const { return lambda == other.lambda; }
    };

    // Constructs a Chirp `void` value.
    Value();
    
    // Factory/Constructors for different kinds of Chirp values
    static Value make_bool(bool val);
    static Value make_int(int64_t val);
    static Value make_string(std::string val);
    static Value make_type(std::shared_ptr<const Type> type_val);
    static Value make_binding(std::shared_ptr<Binding> binding_val);
    static Value make_enumerated_set(std::vector<Value> elements);
    static Value make_lambda(const frontend::LambdaExpr& lambda);

    // In Chirp, every Value has exactly one intrinsic Type tag associated with it.
    std::shared_ptr<const Type> getType() const;

    // Checks and getters
    bool isVoid() const;

    bool isBool() const;
    bool asBool() const;

    bool isInt() const;
    int64_t asInt() const;

    bool isString() const;
    const std::string& asString() const;

    bool isType() const;
    std::shared_ptr<const Type> asType() const;

    bool isBinding() const;
    std::shared_ptr<Binding> asBinding() const;

    bool isEnumeratedSet() const;
    const std::vector<Value>& asEnumeratedSet() const;

    bool isLambda() const;
    const frontend::LambdaExpr& asLambda() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }

    std::string toString() const;

    // Constructor with explicit type and variant payload
    Value(std::shared_ptr<const Type> type, std::variant<std::monostate, bool, int64_t, std::string, TypeTag, BindingTag, EnumeratedSetTag, LambdaTag> payload)
        : type_(std::move(type)), payload_(std::move(payload)) {}

private:
    std::shared_ptr<const Type> type_;
    std::variant<std::monostate, bool, int64_t, std::string, TypeTag, BindingTag, EnumeratedSetTag, LambdaTag> payload_;
};


} // namespace chirp::interpreter
