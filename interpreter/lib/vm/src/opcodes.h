#pragma once
#include <cstdint>

namespace chirp::vm {

enum class Domain : uint8_t {
    Generic = 0b000,
    // Add specific domains later
};

enum class Opcode : uint8_t {
    // Top-Level / Variables
    LoadGlobal = 0x01,
    StoreGlobal = 0x02,
    LoadLocal = 0x03,
    StoreLocal = 0x04,

    // Basic Types
    ConstInt = 0x0A,
    ConstString = 0x0B,

    // Functions & Calls
    Call = 0x0C,
    MakeLambda = 0x0D,
    Return = 0x0F,

    // Consolidated Math & Logic
    BinaryMath = 0x10,
    UnaryMath = 0x11,
    Compare = 0x12,

    // Sets & Ranges
    Union = 0x13,
    Intersect = 0x14,
    MakeRange = 0x15,
    Block = 0x1A,

    // Control Flow
    If = 0x1B,
    Jump = 0x1C,
    JumpIfFalse = 0x1C,
    JumpIfTrue = 0x1D,

    Let = 0x1E,
    Assign = 0x1F
};

enum class OperandType : uint8_t {
    Inline      = 0x00,
    StackLocal  = 0x01,
    Identifier  = 0x02,
    ImmInt      = 0x03,  // Replaces ImmU8/U16/U32/U64
    ImmString   = 0x04,
    ImmChar     = 0x05,
    ImmSymbol   = 0x06,
    // 0x07 is reserved/available
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
