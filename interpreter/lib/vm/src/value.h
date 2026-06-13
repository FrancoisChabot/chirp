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

enum class ValueType { Int, Closure, Null, String, NativeFunc, Char, Symbol, Bool };

struct Value {
    ValueType type = ValueType::Null;
    int64_t as_int = 0;
    std::shared_ptr<Closure> as_closure;
    std::string as_string; // Used for String and Symbol
    std::shared_ptr<NativeFunc> as_native;

    Value() : type(ValueType::Null), as_int(0) {}
    explicit Value(int64_t v) : type(ValueType::Int), as_int(v) {}
    explicit Value(bool b) : type(ValueType::Bool), as_int(b ? 1 : 0) {}
    explicit Value(std::shared_ptr<Closure> c) : type(ValueType::Closure), as_closure(std::move(c)) {}
    explicit Value(std::string s) : type(ValueType::String), as_string(std::move(s)) {}
    explicit Value(NativeFunc f) : type(ValueType::NativeFunc), as_native(std::make_shared<NativeFunc>(std::move(f))) {}
    static Value Char(uint32_t c) { Value v; v.type = ValueType::Char; v.as_int = c; return v; }
    static Value Symbol(std::string s) { Value v; v.type = ValueType::Symbol; v.as_string = std::move(s); return v; }

    std::string toString() const {
        if (type == ValueType::Int) return std::to_string(as_int);
        if (type == ValueType::Bool) return as_int ? "true" : "false";
        if (type == ValueType::Closure) return "<closure>";
        if (type == ValueType::String) return as_string;
        if (type == ValueType::NativeFunc) return "<native fn>";
        if (type == ValueType::Char) return std::string(1, static_cast<char>(as_int)); // Simplified
        if (type == ValueType::Symbol) return "#" + as_string;
        return "null";
    }
};

} // namespace chirp::vm
