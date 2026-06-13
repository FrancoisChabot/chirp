#pragma once

#include <cstdint>
#include <vector>

namespace chirp::vm {

class ProgramUnit {
public:
    ProgramUnit() = default;

    std::vector<uint8_t> bytecode;
    uint32_t num_locals = 0;

    void emit(uint8_t byte) {
        bytecode.push_back(byte);
    }
    
    // Add multiple bytes (useful for multi-byte operands)
    void emit(const std::vector<uint8_t>& bytes) {
        bytecode.insert(bytecode.end(), bytes.begin(), bytes.end());
    }
};

} // namespace chirp::vm
