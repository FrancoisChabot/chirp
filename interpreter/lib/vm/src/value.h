#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include <functional>

namespace chirp::vm {

class ProgramUnit;

struct Closure {
    std::shared_ptr<ProgramUnit> unit;
    // Captures can go here later
};

struct Value;
using NativeFunc = std::function<Value(const std::vector<Value>&)>;

enum class ValueType { Int, Closure, Null, String, NativeFunc };

struct Value {
    ValueType type = ValueType::Null;
    int64_t as_int = 0;
    std::shared_ptr<Closure> as_closure;
    std::string as_string;
    std::shared_ptr<NativeFunc> as_native;

    Value() : type(ValueType::Null), as_int(0) {}
    explicit Value(int64_t v) : type(ValueType::Int), as_int(v) {}
    explicit Value(std::shared_ptr<Closure> c) : type(ValueType::Closure), as_closure(std::move(c)) {}
    explicit Value(std::string s) : type(ValueType::String), as_string(std::move(s)) {}
    explicit Value(NativeFunc f) : type(ValueType::NativeFunc), as_native(std::make_shared<NativeFunc>(std::move(f))) {}

    std::string toString() const {
        if (type == ValueType::Int) return std::to_string(as_int);
        if (type == ValueType::Closure) return "<closure>";
        if (type == ValueType::String) return as_string;
        if (type == ValueType::NativeFunc) return "<native fn>";
        return "null";
    }
};

} // namespace chirp::vm
