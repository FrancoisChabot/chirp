#pragma once

#include <cstdint>
#include <memory>
#include <variant>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "chirp/bigint.h"
#include <map>

namespace chirp::frontend {
class ConstructedSetExpr;
class LambdaExpr;
class SignatureExpr;
}

namespace chirp::interpreter {

class Type;
class Binding;

struct RuntimeScope {
    std::unordered_map<std::string, std::shared_ptr<Binding>> bindings;
    std::vector<std::shared_ptr<Binding>> declaration_order;
};
using RuntimeScopeChain = std::vector<std::shared_ptr<RuntimeScope>>;

class Value {
public:
    struct TypeTag {
        std::shared_ptr<const Type> t;
        bool operator==(const TypeTag& other) const;
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
        std::shared_ptr<Value> start;
        std::shared_ptr<Value> end;
        bool inclusive_end;
        bool operator==(const RangeTag& other) const {
            if (!start || !other.start || !end || !other.end) {
                return start == other.start && end == other.end && inclusive_end == other.inclusive_end;
            }
            return *start == *other.start &&
                *end == *other.end &&
                inclusive_end == other.inclusive_end;
        }
    };
    struct ConstructedSetTag {
        const frontend::ConstructedSetExpr* set;
        std::shared_ptr<const RuntimeScopeChain> captured_scopes;
        bool operator==(const ConstructedSetTag& other) const {
            return set == other.set && captured_scopes == other.captured_scopes;
        }
    };
    struct LambdaTag {
        const frontend::LambdaExpr* lambda;
        std::shared_ptr<const RuntimeScopeChain> captured_scopes;
        bool operator==(const LambdaTag& other) const {
            return lambda == other.lambda && captured_scopes == other.captured_scopes;
        }
    };

    struct SignatureTag {
        const frontend::SignatureExpr* expr;
        std::shared_ptr<const RuntimeScopeChain> captured_scopes;
        bool operator==(const SignatureTag& other) const {
            return expr == other.expr && captured_scopes == other.captured_scopes;
        }
    };

    enum class HostFunction {
        Print,
        Write,
        TypeOf,
        Same,
        Exit,
        Mint,
        MintFinite,
        IsStructType,
        Trait,
        MakeTrait,
        Interface,
        Implements,
        Implementation,
        Implement,
        Register,
        Expect,
        ExpectStdout,
        ExpectStderr,
        ExpectExit,
        ExpectTestFailure,
        IsPure,
        HeapCreate,
        HeapDestroy,
        HeapSharedCreate,
        HeapSharedDestroy,
        Input,
        InjectStdin,
        Invoke,
        FunctionArgs,
        LambdaParamSpace,
        LambdaResultSpace,
        ConstructionArgs,
        Construct,
        TypesInSet,
        IsEnumerable,
        Coextensive
    };
    struct HostFunctionTag {
        HostFunction fn;
        bool operator==(const HostFunctionTag& other) const { return fn == other.fn; }
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
    struct CharTag {
        uint32_t codepoint;
        bool operator==(const CharTag& other) const { return codepoint == other.codepoint; }
    };
    struct ListTag {
        std::shared_ptr<std::vector<Value>> elements;
        bool operator==(const ListTag& other) const;
    };
    struct MintedTag {
        uint64_t id;
        bool operator==(const MintedTag& other) const { return id == other.id; }
    };
    struct TraitTag {
        uint64_t id;
        std::shared_ptr<Value> interface;
        bool operator==(const TraitTag& other) const { return id == other.id; }
    };

    struct StructInstanceTag {
        std::shared_ptr<std::map<std::string, Value>> fields;
        bool operator==(const StructInstanceTag& other) const;
    };
    struct ModuleTag {
        std::string identity;
        std::shared_ptr<std::map<std::string, std::shared_ptr<Binding>>> exports;
        bool operator==(const ModuleTag& other) const { return exports == other.exports; }
    };
    struct HeapAllocationState {
        uint64_t id;
        std::shared_ptr<Value> stored;
        size_t strong_count = 0;
        bool destroyed = false;

        HeapAllocationState(uint64_t id, Value stored);
    };
    struct HeapAllocationTag {
        std::shared_ptr<HeapAllocationState> state;
        bool operator==(const HeapAllocationTag& other) const { return state == other.state; }
    };
    struct EnumFamilyTag {
        uint64_t node_id;
        std::vector<std::string> variants;
        bool operator==(const EnumFamilyTag& other) const { return node_id == other.node_id; }
    };
    struct EnumVariantTag {
        uint64_t enum_node_id;
        std::string variant_name;
        size_t index;
        bool operator==(const EnumVariantTag& other) const {
            return enum_node_id == other.enum_node_id && index == other.index;
        }
    };


    // Constructs a Chirp `void` value.
    Value();
    
    // Factory/Constructors for different kinds of Chirp values
    static Value make_bool(bool val);
    static Value make_int(BigInt val);
    static Value make_char(uint32_t codepoint);
    static Value make_string(std::string val);
    static Value make_type(std::shared_ptr<const Type> type_val);
    static Value make_binding(std::shared_ptr<Binding> binding_val);
    static Value make_enumerated_set(std::vector<Value> elements);
    static Value make_range(Value start, Value end, bool inclusive_end);
    static Value make_constructed_set(const frontend::ConstructedSetExpr& set, std::shared_ptr<const RuntimeScopeChain> captured_scopes = nullptr);
    static Value make_lambda(const frontend::LambdaExpr& lambda, std::shared_ptr<const RuntimeScopeChain> captured_scopes = nullptr);
    static Value make_signature(const frontend::SignatureExpr& expr, std::shared_ptr<const RuntimeScopeChain> captured_scopes = nullptr);
    static Value make_host_function(HostFunction fn);
    static Value make_composite_set(Value left, Value right, CompositeSetOp op);
    static Value make_symbol(std::string name);
    static Value make_list(std::vector<Value> elements);
    static Value make_minted(std::shared_ptr<const Type> type, uint64_t id);
    static Value make_trait(uint64_t id, Value interface);
    static Value make_struct_instance(std::shared_ptr<const Type> type, std::map<std::string, Value> fields);
    static Value make_module(std::string identity, std::map<std::string, std::shared_ptr<Binding>> exports);
    static Value make_heap_allocation(uint64_t id, Value stored);
    static Value make_heap_shared_allocation(uint64_t id, Value stored);
    static Value make_enum_family(uint64_t node_id, std::vector<std::string> variants);
    static Value make_enum_variant(uint64_t enum_node_id, std::string variant_name, size_t index);


    // In Chirp, every Value has exactly one intrinsic Type tag associated with it.
    std::shared_ptr<const Type> getType() const;

    // Checks and getters
    bool isVoid() const;

    bool isBool() const;
    bool asBool() const;

    bool isInt() const;
    const BigInt& asInt() const;

    bool isString() const;
    const std::string& asString() const;

    bool isChar() const;
    uint32_t asChar() const;

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
    const ConstructedSetTag& asConstructedSetTag() const;

    bool isList() const;
    const std::vector<Value>& asList() const;


    bool isLambda() const;
    const frontend::LambdaExpr& asLambda() const;
    const LambdaTag& asLambdaTag() const;

    bool isSignature() const;
    const SignatureTag& asSignatureTag() const;

    bool isHostFunction() const;
    HostFunction asHostFunction() const;

    bool isCompositeSet() const;
    const CompositeSetTag& asCompositeSet() const;

    bool isMinted() const;
    uint64_t asMintedId() const;

    bool isTrait() const;
    uint64_t asTraitId() const;
    const Value& asTraitInterface() const;

    bool isStructInstance() const;
    const StructInstanceTag& asStructInstance() const;

    bool isModule() const;
    const ModuleTag& asModule() const;

    bool isHeapAllocation() const;
    const HeapAllocationTag& asHeapAllocation() const;

    bool isEnumFamily() const;
    const EnumFamilyTag& asEnumFamily() const;
    bool isEnumVariant() const;
    const EnumVariantTag& asEnumVariant() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }

    std::string toString() const;

    // Constructor with explicit type and variant payload
    using Payload = std::variant<std::monostate, bool, BigInt, std::string, TypeTag, BindingTag, EnumeratedSetTag, RangeTag, ConstructedSetTag, LambdaTag, HostFunctionTag, CompositeSetTag, SymbolTag, CharTag, ListTag, MintedTag, TraitTag, StructInstanceTag, ModuleTag, HeapAllocationTag, EnumFamilyTag, EnumVariantTag, SignatureTag>;


    Value(std::shared_ptr<const Type> type, Payload payload)
        : type_(std::move(type)), payload_(std::move(payload)) {}

private:
    std::shared_ptr<const Type> type_;
    Payload payload_;
};


} // namespace chirp::interpreter
