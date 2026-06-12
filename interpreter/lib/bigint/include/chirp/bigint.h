#pragma once

#include <string>
#include <string_view>
#include <ostream>
#include <compare>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace chirp::interpreter {

class BigInt {
public:
    // Constructors
    BigInt();
    BigInt(int64_t value);
    explicit BigInt(std::string_view str);

    // Copy / Move constructors and assignment
    BigInt(const BigInt&) = default;
    BigInt(BigInt&&) noexcept = default;
    BigInt& operator=(const BigInt&) = default;
    BigInt& operator=(BigInt&&) noexcept = default;

    // Formatting
    std::string to_string() const;

    // Unary Operators
    BigInt operator-() const;
    BigInt operator+() const;

    // Binary Arithmetic Operators
    BigInt operator+(const BigInt& other) const;
    BigInt operator-(const BigInt& other) const;
    BigInt operator*(const BigInt& other) const;
    BigInt operator/(const BigInt& other) const;
    BigInt operator%(const BigInt& other) const;

    // Compound Assignment Operators
    BigInt& operator+=(const BigInt& other);
    BigInt& operator-=(const BigInt& other);
    BigInt& operator*=(const BigInt& other);
    BigInt& operator/=(const BigInt& other);
    BigInt& operator%=(const BigInt& other);

    bool operator==(const BigInt& other) const;
    std::strong_ordering operator<=>(const BigInt& other) const;

    // Friend stream insertion
    friend std::ostream& operator<<(std::ostream& os, const BigInt& val);

    // Conversion and range validation helpers
    bool fits_int64() const;
    int64_t to_int64() const;

    bool fits_uint64() const;
    uint64_t to_uint64() const;

private:
    struct LargeValue {
        bool negative = false;
        std::vector<uint32_t> limbs;

        bool operator==(const LargeValue& other) const = default;
    };

    using Storage = std::variant<int64_t, LargeValue>;

    explicit BigInt(Storage storage);

    bool is_small() const;
    int64_t small_value() const;
    const LargeValue& large_value() const;
    std::pair<bool, std::vector<uint32_t>> signed_magnitude() const;

    static BigInt from_signed_magnitude(bool negative, std::vector<uint32_t> limbs);

    Storage storage_;
};

} // namespace chirp::interpreter
