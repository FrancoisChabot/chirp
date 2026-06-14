#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <optional>
#include <vector>

#include <functional>
#include <unordered_map>

namespace chirp::vm {

class ProgramUnit;

struct Value;
struct CallArgument;
using NativeFunc = std::function<Value(const std::vector<CallArgument>&)>;
struct Closure;
struct StructTypeDef;
struct TypeValueDef;
struct TraitDef;
struct SignatureDef;
struct ConstructedSetDef;
struct CompositeSetDef;
struct HeapState;
struct EnumFamilyDef;
struct EnumVariantDef;

struct RangeDef {
    std::shared_ptr<Value> start;
    std::shared_ptr<Value> end;
    bool inclusive_end;
};

enum class ValueType {
    Int,
    Closure,
    Null,
    String,
    NativeFunc,
    Char,
    Symbol,
    Bool,
    Struct,
    Array,
    StructType,
    TypeValue,
    Minted,
    Trait,
    Signature,
    EnumeratedSet,
    ConstructedSet,
    CompositeSet,
    Heap,
    EnumFamily,
    EnumVariant,
    Range
};

struct Value {
    ValueType type = ValueType::Null;
    int64_t as_int = 0;
    uint64_t as_id = 0;
    std::shared_ptr<Closure> as_closure;
    std::string as_string; // Used for String and Symbol
    std::shared_ptr<NativeFunc> as_native;
    std::shared_ptr<std::unordered_map<std::string, Value>> as_struct;
    std::shared_ptr<std::vector<Value>> as_array;
    std::shared_ptr<StructTypeDef> as_struct_type;
    std::shared_ptr<StructTypeDef> as_struct_instance_type;
    std::shared_ptr<TypeValueDef> as_type_value;
    std::shared_ptr<TraitDef> as_trait;
    std::shared_ptr<SignatureDef> as_signature;
    std::shared_ptr<std::vector<Value>> as_set_elements;
    std::shared_ptr<ConstructedSetDef> as_constructed_set;
    std::shared_ptr<CompositeSetDef> as_composite_set;
    std::shared_ptr<HeapState> as_heap;
    std::shared_ptr<EnumFamilyDef> as_enum_family;
    std::shared_ptr<EnumVariantDef> as_enum_variant;
    std::shared_ptr<RangeDef> as_range;

    Value() : type(ValueType::Null), as_int(0) {}
    explicit Value(int64_t v) : type(ValueType::Int), as_int(v) {}
    explicit Value(bool b) : type(ValueType::Bool), as_int(b ? 1 : 0) {}
    explicit Value(std::shared_ptr<Closure> c) : type(ValueType::Closure), as_closure(std::move(c)) {}
    explicit Value(std::string s) : type(ValueType::String), as_string(std::move(s)) {}
    explicit Value(NativeFunc f) : type(ValueType::NativeFunc), as_native(std::make_shared<NativeFunc>(std::move(f))) {}
    static Value Char(uint32_t c) { Value v; v.type = ValueType::Char; v.as_int = c; return v; }
    static Value Symbol(std::string s) { Value v; v.type = ValueType::Symbol; v.as_string = std::move(s); return v; }
    static Value Struct(std::shared_ptr<std::unordered_map<std::string, Value>> s,
                        std::shared_ptr<StructTypeDef> type = nullptr) {
        Value v;
        v.type = ValueType::Struct;
        v.as_struct = std::move(s);
        v.as_struct_instance_type = std::move(type);
        return v;
    }
    static Value Array(std::shared_ptr<std::vector<Value>> a) { Value v; v.type = ValueType::Array; v.as_array = std::move(a); return v; }
    static Value StructType(std::shared_ptr<StructTypeDef> s) { Value v; v.type = ValueType::StructType; v.as_struct_type = std::move(s); return v; }
    static Value Type(std::shared_ptr<TypeValueDef> t) { Value v; v.type = ValueType::TypeValue; v.as_type_value = std::move(t); return v; }
    static Value Minted(std::shared_ptr<TypeValueDef> t, uint64_t id) {
        Value v;
        v.type = ValueType::Minted;
        v.as_type_value = std::move(t);
        v.as_id = id;
        return v;
    }
    static Value Trait(std::shared_ptr<TraitDef> t) { Value v; v.type = ValueType::Trait; v.as_trait = std::move(t); return v; }
    static Value Signature(std::shared_ptr<SignatureDef> s) { Value v; v.type = ValueType::Signature; v.as_signature = std::move(s); return v; }
    static Value EnumeratedSet(std::shared_ptr<std::vector<Value>> elements) {
        Value v;
        v.type = ValueType::EnumeratedSet;
        v.as_set_elements = std::move(elements);
        return v;
    }
    static Value ConstructedSet(std::shared_ptr<ConstructedSetDef> set) {
        Value v;
        v.type = ValueType::ConstructedSet;
        v.as_constructed_set = std::move(set);
        return v;
    }
    static Value CompositeSet(std::shared_ptr<CompositeSetDef> set) {
        Value v;
        v.type = ValueType::CompositeSet;
        v.as_composite_set = std::move(set);
        return v;
    }
    static Value Heap(std::shared_ptr<HeapState> heap, std::shared_ptr<TypeValueDef> heap_type) {
        Value v;
        v.type = ValueType::Heap;
        v.as_heap = std::move(heap);
        v.as_type_value = std::move(heap_type);
        return v;
    }
    static Value EnumFamily(std::shared_ptr<EnumFamilyDef> family) {
        Value v;
        v.type = ValueType::EnumFamily;
        v.as_enum_family = std::move(family);
        return v;
    }
    static Value EnumVariant(std::shared_ptr<EnumVariantDef> variant) {
        Value v;
        v.type = ValueType::EnumVariant;
        v.as_enum_variant = std::move(variant);
        return v;
    }
    static Value Range(std::shared_ptr<Value> start, std::shared_ptr<Value> end, bool inclusive_end) {
        Value v;
        v.type = ValueType::Range;
        v.as_range = std::make_shared<RangeDef>();
        v.as_range->start = std::move(start);
        v.as_range->end = std::move(end);
        v.as_range->inclusive_end = inclusive_end;
        return v;
    }

    std::string toString() const {
        if (type == ValueType::Int) return std::to_string(as_int);
        if (type == ValueType::Bool) return as_int ? "true" : "false";
        if (type == ValueType::Closure) return "<closure>";
        if (type == ValueType::String) return as_string;
        if (type == ValueType::NativeFunc) return "<native fn>";
        if (type == ValueType::Char) return std::string(1, static_cast<char>(as_int)); // Simplified
        if (type == ValueType::Symbol) return "#" + as_string;
        if (type == ValueType::Struct) return "<struct>";
        if (type == ValueType::Array) return "<array>";
        if (type == ValueType::StructType) return "<struct type>";
        if (type == ValueType::TypeValue) return "<type>";
        if (type == ValueType::Minted) return "<minted>";
        if (type == ValueType::Trait) return "<trait>";
        if (type == ValueType::Signature) return "<signature>";
        if (type == ValueType::EnumeratedSet) return "<set>";
        if (type == ValueType::ConstructedSet) return "<constructed set>";
        if (type == ValueType::CompositeSet) return "<composite set>";
        if (type == ValueType::Heap) return "<heap allocation>";
        if (type == ValueType::EnumFamily) return "<enum family>";
        if (type == ValueType::EnumVariant) return as_string;
        if (type == ValueType::Range) return as_range->start->toString() + (as_range->inclusive_end ? "..=" : "..") + as_range->end->toString();
        return "null";
    }
};

struct Closure {
    std::shared_ptr<ProgramUnit> unit;
    std::vector<Value> captures;
};

struct StructFieldSpec {
    std::string name;
    std::shared_ptr<Closure> constraint;
    std::shared_ptr<Closure> default_value;
};

struct StructTypeDef {
    std::vector<StructFieldSpec> fields;
};

struct TypeValueDef {
    enum class Kind {
        Primitive,
        Meta,
        Lambda,
        Trait,
        Finite,
    };

    Kind kind = Kind::Primitive;
    std::string name;
    uint64_t id = 0;
    uint64_t finite_count = 0;
};

struct TraitDef {
    uint64_t id = 0;
    Value interface;
};

struct SignatureParameterSpec {
    std::string name;
    std::shared_ptr<Closure> constraint;
};

struct SignatureDef {
    std::vector<SignatureParameterSpec> parameters;
    std::shared_ptr<Closure> return_bound;
};

struct ConstructedSetDef {
    std::shared_ptr<Closure> bound;
    std::shared_ptr<Closure> predicate;
};

struct CompositeSetDef {
    enum class Op {
        Union,
        Intersection,
    };

    std::shared_ptr<Value> left;
    std::shared_ptr<Value> right;
    Op op = Op::Union;
};

struct HeapState {
    uint64_t id = 0;
    Value stored;
    bool shared = false;
    bool destroyed = false;
    uint64_t strong_count = 1;
};

struct CallArgument {
    std::optional<std::string> name;
    Value value;
};

struct EnumFamilyDef {
    uint64_t node_id;
    std::vector<std::string> variants;
};

struct EnumVariantDef {
    uint64_t enum_node_id;
    std::string variant_name;
    size_t index;
};

} // namespace chirp::vm
