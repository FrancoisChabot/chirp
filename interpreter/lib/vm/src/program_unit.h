#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace chirp::vm {

enum class CaptureSourceKind : uint8_t {
    Local,
    Capture,
};

struct CaptureSource {
    CaptureSourceKind kind;
    uint32_t index;
};

class ProgramUnit {
public:
    ProgramUnit() = default;

    std::vector<uint8_t> bytecode;
    uint32_t num_locals = 0;
    
    std::vector<std::string> constant_strings;
    std::vector<std::string> parameter_names;
    std::vector<std::shared_ptr<ProgramUnit>> child_units;
    std::vector<CaptureSource> captures;

    void emit(uint8_t byte) {
        bytecode.push_back(byte);
    }
    
    void emit(const std::vector<uint8_t>& bytes) {
        bytecode.insert(bytecode.end(), bytes.begin(), bytes.end());
    }

    uint32_t addStringConstant(const std::string& str) {
        constant_strings.push_back(str);
        return constant_strings.size() - 1;
    }

    uint32_t addChildUnit(std::shared_ptr<ProgramUnit> unit) {
        child_units.push_back(unit);
        return child_units.size() - 1;
    }
};

} // namespace chirp::vm
