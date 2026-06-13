#pragma once

#include <cstdint>
#include <string>

namespace chirp::vm {

struct Value {
    int64_t as_int;
    
    // Minimal constructor for now
    explicit Value(int64_t v = 0) : as_int(v) {}

    // A very simple toString for debugging
    std::string toString() const {
        return std::to_string(as_int);
    }
};

} // namespace chirp::vm
