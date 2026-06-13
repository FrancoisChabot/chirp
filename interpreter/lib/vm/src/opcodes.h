#pragma once

#include <cstdint>

namespace chirp::vm {

enum class Opcode : uint8_t {
    // Control Flow & Scope
    Block = 0x01,
    Break = 0x02,
    If = 0x03,
    Loop = 0x04,
    Match = 0x05,

    // Memory & Reference
    Ref = 0x06,
    Deref = 0x07,
    GetField = 0x08,
    SetField = 0x09,
    Index = 0x0A,
    SetIndex = 0x0B,

    // Functions & Calls
    Call = 0x0C,
    MakeLambda = 0x0D,
    Intrinsic = 0x0E,
    Return = 0x0F,

    // Consolidated Math & Logic
    BinaryMath = 0x10,
    UnaryMath = 0x11,
    Compare = 0x12,

    // Sets & Ranges
    Union = 0x13,
    Intersect = 0x14,
    MakeRange = 0x15,
    MakeConstructedSet = 0x16,

    // Data Structures
    MakeStructDef = 0x17,
    MakeEnumDef = 0x18,
    MakeList = 0x19,
    MakeEnumeratedSet = 0x1A,
    MakeAnonStruct = 0x1B,

    // Statements
    Let = 0x1E,
    Assign = 0x1F,
};

enum class Domain : uint8_t {
    Generic = 0b000,
    D1 = 0b001,
    D2 = 0b010,
    D3 = 0b011,
    D4 = 0b100,
    D5 = 0b101,
    D6 = 0b110,
    LocalExt = 0b111,
};

enum class OperandType : uint8_t {
    Inline = 0x00,
    StackLocal = 0x01,
    Identifier = 0x02,
    Immediate = 0x03,
    ImmU8 = 0x04,
    ImmU16 = 0x05,
    ImmU32 = 0x06,
    ImmU64 = 0x07,
    ImmString = 0x08,
};

enum class BinaryMathOp : uint8_t {
    Add = 0x00,
    Sub = 0x01,
    Mul = 0x02,
    Div = 0x03,
    Mod = 0x04,
};

enum class UnaryMathOp : uint8_t {
    Negate = 0x00,
    Not = 0x01,
    BitNot = 0x02,
};

enum class CompareOp : uint8_t {
    Eq = 0x00,
    Neq = 0x01,
    Lt = 0x02,
    Lte = 0x03,
    Gt = 0x04,
    Gte = 0x05,
};

inline uint8_t encodeInstruction(Opcode op, Domain domain) {
    return (static_cast<uint8_t>(op) << 3) | static_cast<uint8_t>(domain);
}

inline Opcode decodeOpcode(uint8_t inst) {
    return static_cast<Opcode>(inst >> 3);
}

inline Domain decodeDomain(uint8_t inst) {
    return static_cast<Domain>(inst & 0b111);
}

} // namespace chirp::vm
