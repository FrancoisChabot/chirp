#pragma once

#include <string>
#include <string_view>
#include <span>

namespace chirp {

class vm;
class Value;
class Nature;

using IntrinsicFunctionPtr = Value(*)(vm&, std::span<const Value>);
using nature_key = std::string;
using NatureRef = const Nature*;

class Nature {
public:
    virtual ~Nature();
    virtual std::string_view name() const = 0;
};

class IntNature : public Nature {
public:
    std::string_view name() const override { return "int"; }
};

class BoolNature : public Nature {
public:
    std::string_view name() const override { return "bool"; }
};

class StringNature : public Nature {
public:
    std::string_view name() const override { return "string"; }
};

class NatureNature : public Nature {
public:
    std::string_view name() const override { return "nature"; }
};

class IntrinsicFunctionNature : public Nature {
public:
    std::string_view name() const override { return "IntrinsicFunction"; }
};

} // namespace chirp
