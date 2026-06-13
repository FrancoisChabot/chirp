#pragma once
#include <cstdint>

namespace chirp::vm {

enum class Domain : uint8_t {
    Generic = 0b000,
    // Add specific domains later
};

enum class Opcode : uint8_t {
    Eval = 0x00,
    Block = 0x01,
    Break = 0x02,

    // Functions & Calls
    Call = 0x0C,
    MakeLambda = 0x0D,
    Return = 0x0E,

    // Memory & Reference
    GetField = 0x08,
    Index = 0x09,

    // Consolidated Math & Logic
    BinaryMath = 0x0F,
    UnaryMath = 0x10,
    Compare = 0x11,

    // Sets & Ranges
    Union = 0x12,
    Intersect = 0x13,
    MakeRange = 0x14,

    // Control Flow
    If = 0x15,

    Let = 0x1E,
    Assign = 0x1F
};

enum class OperandType : uint8_t {
    Inline      = 0x00,
    StackLocal  = 0x01,
    Identifier  = 0x02,
    ImmInt      = 0x03,
    ImmString   = 0x04,
    ImmChar     = 0x05,
    ImmSymbol   = 0x06,
    ImmNull     = 0x07,
    Capture     = 0x08,
};

enum class BinaryMathOp : uint8_t {
    Add = 0, Sub = 1, Mul = 2, Div = 3, Mod = 4
};

enum class CompareOp : uint8_t {
    Eq = 0, Neq = 1, Lt = 2, Lte = 3, Gt = 4, Gte = 5
};

inline uint8_t encodeInstruction(Opcode op, Domain dom) {
    return (static_cast<uint8_t>(op) << 3) | static_cast<uint8_t>(dom);
}

inline Opcode decodeOpcode(uint8_t instr) {
    return static_cast<Opcode>(instr >> 3);
}

inline Domain decodeDomain(uint8_t instr) {
    return static_cast<Domain>(instr & 0b111);
}

} // namespace chirp::vm
