#pragma once

#include <string>
#include <string_view>
#include <ostream>
#include <compare>
#include <cstdint>

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

    // Comparison Operators (C++20 spaceship operator)
    bool operator==(const BigInt& other) const = default;
    std::strong_ordering operator<=>(const BigInt& other) const {
        if (value_ < other.value_) return std::strong_ordering::less;
        if (value_ > other.value_) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    // Friend stream insertion
    friend std::ostream& operator<<(std::ostream& os, const BigInt& val);

    // Conversion and range validation helpers
    bool fits_int64() const;
    int64_t to_int64() const;

    bool fits_uint64() const;
    uint64_t to_uint64() const;

private:
#ifdef __SIZEOF_INT128__
    __int128_t value_;
#else
    long long value_;
#endif
};

} // namespace chirp::interpreter
