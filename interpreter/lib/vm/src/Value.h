#pragma once

#include <variant>
#include <string>
#include <string_view>
#include "nature.h"

namespace chirp {

class Value {
public:
    Value(); // defaults to bool(false) with nullptr nature
    
    Value(int val, NatureRef nature);
    Value(bool val, NatureRef nature);
    Value(std::string val, NatureRef nature);
    Value(const char* val, NatureRef nature);
    Value(NatureRef nature_val, NatureRef nature_nature);
    Value(TraitRef trait_val, NatureRef trait_nature);
    Value(IntrinsicFunctionPtr func, NatureRef nature_nature);

    bool isInt() const;
    int asInt() const;

    bool isString() const;
    std::string_view asString() const;

    bool isBool() const;
    bool asBool() const;

    bool isNature() const;
    NatureRef asNature() const;

    bool isTrait() const;
    TraitRef asTrait() const;

    bool isIntrinsicFunction() const;
    IntrinsicFunctionPtr asIntrinsicFunction() const;

    NatureRef getNature() const;

    bool operator==(const Value& other) const;

private:
    std::variant<int, bool, std::string, NatureRef, TraitRef, IntrinsicFunctionPtr> storage_;
    NatureRef nature_;
};

} // namespace chirp
