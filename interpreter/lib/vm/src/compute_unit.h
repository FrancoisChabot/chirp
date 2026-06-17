#pragma once

#include <vector>

namespace chirp {

enum class opcode {
    noop,
};

struct instruction {
    opcode op = opcode::noop;
};

class compute_unit {
public:
    compute_unit() = default;
    ~compute_unit() = default;

    // TODO: This vector of instructions is to be replaced with a linear binary tape eventually.
    std::vector<instruction> instructions;
};

} // namespace chirp
