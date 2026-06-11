#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include "value.h"

namespace chirp::frontend {
class Expr;
class StructExpr;
}

namespace chirp::interpreter {

class Type {
public:
    virtual ~Type() = default;

    virtual std::string_view name() const = 0;
    virtual bool equals(const Type& other) const { return this == &other; }

    // Checks if instances of this type can act as sets.
    virtual bool hasSetness() const { return false; }

    // Belonging predicate: typeof(S).belongs(S, v)
    // S is the set instance (which is a Value of this Type).
    // v is the value being checked for belonging.
    virtual Value belongs(const Value& S, const Value& v) const;

    // Belonging approx: typeof(S).belongs_approx(S, lc)
    virtual Value belongs_approx(const Value& S, const Value& lc) const;
};

// Subclass for the type of all types (Type)
class MetaType : public Type {
public:
    std::string_view name() const override { return "Type"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

// Subclass for Bool
class BoolType : public Type {
public:
    std::string_view name() const override { return "bool"; }
};





// Subclass for VoidType
class VoidType : public Type {
public:
    std::string_view name() const override { return "Void"; }
};

// Subclass for EnumeratedSetType
class EnumeratedSetType : public Type {
public:
    std::string_view name() const override { return "EnumeratedSet"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

class RangeType : public Type {
public:
    std::string_view name() const override { return "Range"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

class ConstructedSetType : public Type {
public:
    std::string_view name() const override { return "ConstructedSet"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

class CompositeSetType : public Type {
public:
    std::string_view name() const override { return "CompositeSet"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

// Subclass for BindingType
class BindingType : public Type {
public:
    std::string_view name() const override { return "Binding"; }
};

class FunctionType : public Type {
public:
    std::string_view name() const override { return "Function"; }
};

class SignatureType : public Type {
public:
    std::string_view name() const override { return "Signature"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

// Additional convenient types for interpreter literals
class IntType : public Type {
public:
    std::string_view name() const override { return "int"; }
};

class CharType : public Type {
public:
    std::string_view name() const override { return "char"; }
};

class StringType : public Type {
public:
    std::string_view name() const override { return "string"; }
};

class SymbolType : public Type {
public:
    std::string_view name() const override { return "Symbol"; }
};

class ListType : public Type {
public:
    std::string_view name() const override { return "List"; }
};

class TraitType : public Type {
public:
    std::string_view name() const override { return "Trait"; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

class ModuleType : public Type {
public:
    std::string_view name() const override { return "Module"; }
};

class HeapAllocationType : public Type {
public:
    std::string_view name() const override { return "HeapAllocation"; }
};

class HeapSharedAllocationType : public Type {
public:
    std::string_view name() const override { return "HeapSharedAllocation"; }
};

class MintedType : public Type {
public:
    explicit MintedType(uint64_t id);

    std::string_view name() const override { return name_; }
    uint64_t id() const { return id_; }

private:
    uint64_t id_;
    std::string name_;
};

class StructType : public Type {
public:
    explicit StructType(const frontend::StructExpr* expr) : expr_(expr) {}
    std::string_view name() const override { return "Struct"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;

    const frontend::StructExpr* expr() const { return expr_; }
private:
    const frontend::StructExpr* expr_;
};

struct OrderedStructFieldSpec {
    std::string name;
    bool is_mut = false;
    bool is_final = false;
    const frontend::Expr* type_bound = nullptr;
    const frontend::Expr* initializer = nullptr;
};

class RuntimeStructType : public Type {
public:
    explicit RuntimeStructType(std::vector<OrderedStructFieldSpec> fields)
        : fields_(std::move(fields)) {}

    std::string_view name() const override { return "Struct"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;

    const std::vector<OrderedStructFieldSpec>& fields() const { return fields_; }

private:
    std::vector<OrderedStructFieldSpec> fields_;
};

class EnumFamilyType : public Type {
public:
    std::string_view name() const override { return "EnumFamily"; }
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;
};

class EnumVariantType : public Type {
public:
    virtual std::string_view name() const override { return "EnumVariant"; }
};

class ReferenceType : public Type {
public:
    ReferenceType(Value target_type, bool is_mut)
        : target_type_(std::move(target_type)), is_mut_(is_mut) {}

    std::string_view name() const override;
    bool equals(const Type& other) const override;
    bool hasSetness() const override { return true; }
    Value belongs(const Value& S, const Value& v) const override;
    Value belongs_approx(const Value& S, const Value& lc) const override;

    const Value& target_type() const { return target_type_; }
    bool is_mut() const { return is_mut_; }

private:
    Value target_type_;
    bool is_mut_;
    mutable std::string name_;
};

} // namespace chirp::interpreter
