#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace chirp::vm {

class ProgramUnit;

struct Closure {
    std::shared_ptr<ProgramUnit> unit;
    // Captures can go here later
};

enum class ValueType { Int, Closure, Null, Intrinsic };

struct Value {
    ValueType type = ValueType::Null;
    int64_t as_int = 0;
    std::shared_ptr<Closure> as_closure;
    std::string as_intrinsic;

    Value() : type(ValueType::Null), as_int(0) {}
    explicit Value(int64_t v) : type(ValueType::Int), as_int(v) {}
    explicit Value(std::shared_ptr<Closure> c) : type(ValueType::Closure), as_closure(std::move(c)) {}
    static Value Intrinsic(std::string name) {
        Value v;
        v.type = ValueType::Intrinsic;
        v.as_intrinsic = std::move(name);
        return v;
    }

    std::string toString() const {
        if (type == ValueType::Int) return std::to_string(as_int);
        if (type == ValueType::Closure) return "<closure>";
        if (type == ValueType::Intrinsic) return "<intrinsic " + as_intrinsic + ">";
        return "null";
    }
};

} // namespace chirp::vm
