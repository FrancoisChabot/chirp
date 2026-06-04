#pragma once

#include <memory>
#include <variant>
#include <string>
#include <string_view>
#include <vector>

namespace chirp::frontend {
class ConstructedSetExpr;
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
    struct RangeTag {
        int64_t start;
        int64_t end;
        bool inclusive_end;
        bool operator==(const RangeTag& other) const {
            return start == other.start &&
                end == other.end &&
                inclusive_end == other.inclusive_end;
        }
    };
    struct ConstructedSetTag {
        const frontend::ConstructedSetExpr* set;
        bool operator==(const ConstructedSetTag& other) const { return set == other.set; }
    };
    struct LambdaTag {
        const frontend::LambdaExpr* lambda;
        bool operator==(const LambdaTag& other) const { return lambda == other.lambda; }
    };

    enum class CompositeSetOp { Union, Intersection };
    struct CompositeSetTag {
        std::shared_ptr<Value> left;
        std::shared_ptr<Value> right;
        CompositeSetOp op;
        bool operator==(const CompositeSetTag& other) const {
            return left == other.left && right == other.right && op == other.op;
        }
    };
    struct SymbolTag {
        std::string name;
        bool operator==(const SymbolTag& other) const { return name == other.name; }
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
    static Value make_range(int64_t start, int64_t end, bool inclusive_end);
    static Value make_constructed_set(const frontend::ConstructedSetExpr& set);
    static Value make_lambda(const frontend::LambdaExpr& lambda);
    static Value make_composite_set(Value left, Value right, CompositeSetOp op);
    static Value make_symbol(std::string name);

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

    bool isSymbol() const;
    const std::string& asSymbol() const;

    bool isType() const;
    std::shared_ptr<const Type> asType() const;

    bool isBinding() const;
    std::shared_ptr<Binding> asBinding() const;

    bool isEnumeratedSet() const;
    const std::vector<Value>& asEnumeratedSet() const;

    bool isRange() const;
    RangeTag asRange() const;

    bool isConstructedSet() const;
    const frontend::ConstructedSetExpr& asConstructedSet() const;

    bool isLambda() const;
    const frontend::LambdaExpr& asLambda() const;

    bool isCompositeSet() const;
    const CompositeSetTag& asCompositeSet() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }

    std::string toString() const;

    // Constructor with explicit type and variant payload
    using Payload = std::variant<std::monostate, bool, int64_t, std::string, TypeTag, BindingTag, EnumeratedSetTag, RangeTag, ConstructedSetTag, LambdaTag, CompositeSetTag, SymbolTag>;

    Value(std::shared_ptr<const Type> type, Payload payload)
        : type_(std::move(type)), payload_(std::move(payload)) {}

private:
    std::shared_ptr<const Type> type_;
    Payload payload_;
};


} // namespace chirp::interpreter
