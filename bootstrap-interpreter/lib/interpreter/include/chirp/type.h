#pragma once

#include <string_view>
#include <memory>
#include "value.h"

namespace chirp::interpreter {

class Type {
public:
    virtual ~Type() = default;

    virtual std::string_view name() const = 0;

    // Checks if instances of this type can act as sets.
    virtual bool hasSetness() const { return false; }

    // Belonging predicate: typeof(S).bp(S, v)
    // S is the set instance (which is a Value of this Type).
    // v is the value being checked for belonging.
    virtual Value bp(const Value& S, const Value& v) const;

    // Belonging range: typeof(S).br(S, lc)
    virtual Value br(const Value& S, const Value& lc) const;
};

// Subclass for the type of all types (Type)
class MetaType : public Type {
public:
    std::string_view name() const override { return "Type"; }
    bool hasSetness() const override { return true; }
    Value bp(const Value& S, const Value& v) const override;
    Value br(const Value& S, const Value& lc) const override;
};

// Subclass for Bool
class BoolType : public Type {
public:
    std::string_view name() const override { return "Bool"; }
};

// Subclass for AnyType
class AnyType : public Type {
public:
    std::string_view name() const override { return "AnyType"; }
    bool hasSetness() const override { return true; }
    Value bp(const Value& S, const Value& v) const override;
    Value br(const Value& S, const Value& lc) const override;
};

// Subclass for EmptyType
class EmptyType : public Type {
public:
    std::string_view name() const override { return "EmptyType"; }
    bool hasSetness() const override { return true; }
    Value bp(const Value& S, const Value& v) const override;
    Value br(const Value& S, const Value& lc) const override;
};

// Subclass for SetType
class SetType : public Type {
public:
    std::string_view name() const override { return "SetType"; }
    bool hasSetness() const override { return true; }
    Value bp(const Value& S, const Value& v) const override;
    Value br(const Value& S, const Value& lc) const override;
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
    Value bp(const Value& S, const Value& v) const override;
    Value br(const Value& S, const Value& lc) const override;
};

// Subclass for BindingType
class BindingType : public Type {
public:
    std::string_view name() const override { return "Binding"; }
};

// Additional convenient types for interpreter literals
class IntType : public Type {
public:
    std::string_view name() const override { return "int"; }
};

class StringType : public Type {
public:
    std::string_view name() const override { return "string"; }
};


} // namespace chirp::interpreter
