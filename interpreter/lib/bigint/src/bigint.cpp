#include "chirp/bigint.h"
#include <stdexcept>
#include <algorithm>
#include <limits>

namespace chirp::interpreter {

BigInt::BigInt() : value_(0) {}

BigInt::BigInt(int64_t value) : value_(value) {}

BigInt::BigInt(std::string_view str) {
    if (str.empty()) {
        throw std::invalid_argument("Empty string is not a valid BigInt");
    }

    size_t idx = 0;
    bool negative = false;
    if (str[0] == '-') {
        negative = true;
        idx = 1;
    } else if (str[0] == '+') {
        idx = 1;
    }

    if (idx == str.size()) {
        throw std::invalid_argument("Invalid BigInt: sign without digits in \"" + std::string(str) + "\"");
    }

    unsigned __int128 uval = 0;
    for (; idx < str.size(); ++idx) {
        char c = str[idx];
        if (c < '0' || c > '9') {
            throw std::invalid_argument("Invalid character in BigInt: \"" + std::string(str) + "\"");
        }
        int digit = c - '0';

        // Overflow bounds for __int128_t:
        // Negative min: -170141183460469231731687303715884105728 => magnitude 2^127
        // Positive max: 170141183460469231731687303715884105727 => magnitude 2^127 - 1
        unsigned __int128 limit = ((unsigned __int128)1 << 127);
        if (!negative) {
            limit -= 1;
        }

        if (uval > (limit - digit) / 10) {
            throw std::out_of_range("BigInt overflow: \"" + std::string(str) + "\"");
        }
        uval = uval * 10 + digit;
    }

    if (negative) {
        value_ = -static_cast<__int128_t>(uval);
    } else {
        value_ = static_cast<__int128_t>(uval);
    }
}

std::string BigInt::to_string() const {
    if (value_ == 0) {
        return "0";
    }

    std::string s;
    __int128_t temp = value_;
    bool negative = false;
    if (temp < 0) {
        negative = true;
    }

    unsigned __int128 utemp = temp < 0 ? -static_cast<unsigned __int128>(temp) : static_cast<unsigned __int128>(temp);
    while (utemp > 0) {
        s.push_back('0' + (utemp % 10));
        utemp /= 10;
    }

    if (negative) {
        s.push_back('-');
    }

    std::reverse(s.begin(), s.end());
    return s;
}

BigInt BigInt::operator-() const {
    // Handling overflow on MIN values is standard C++ UB if done naively.
    // __int128_t MIN negated is mathematically 2^127, which overflows signed __int128_t positive limit.
    constexpr __int128_t INT128_MIN = ~((__int128_t)0) == 0 ? 0 : -((__int128_t)1 << 126) - ((__int128_t)1 << 126);
    if (value_ == INT128_MIN) {
        throw std::out_of_range("Overflow negating BigInt minimum value");
    }
    return BigInt(-value_);
}

BigInt BigInt::operator+() const {
    return *this;
}

BigInt BigInt::operator+(const BigInt& other) const {
    // Safe addition overflow check
    if (other.value_ > 0 && value_ > std::numeric_limits<__int128_t>::max() - other.value_) {
        throw std::out_of_range("BigInt addition overflow");
    }
    if (other.value_ < 0 && value_ < std::numeric_limits<__int128_t>::min() - other.value_) {
        throw std::out_of_range("BigInt addition underflow");
    }
    return BigInt(value_ + other.value_);
}

BigInt BigInt::operator-(const BigInt& other) const {
    // Safe subtraction overflow check
    if (other.value_ < 0 && value_ > std::numeric_limits<__int128_t>::max() + other.value_) {
        throw std::out_of_range("BigInt subtraction overflow");
    }
    if (other.value_ > 0 && value_ < std::numeric_limits<__int128_t>::min() + other.value_) {
        throw std::out_of_range("BigInt subtraction underflow");
    }
    return BigInt(value_ - other.value_);
}

BigInt BigInt::operator*(const BigInt& other) const {
    if (value_ == 0 || other.value_ == 0) {
        return BigInt(0);
    }
    if (value_ > 0) {
        if (other.value_ > 0) {
            if (value_ > std::numeric_limits<__int128_t>::max() / other.value_) {
                throw std::out_of_range("BigInt multiplication overflow");
            }
        } else {
            if (other.value_ < std::numeric_limits<__int128_t>::min() / value_) {
                throw std::out_of_range("BigInt multiplication underflow");
            }
        }
    } else {
        if (other.value_ > 0) {
            if (value_ < std::numeric_limits<__int128_t>::min() / other.value_) {
                throw std::out_of_range("BigInt multiplication underflow");
            }
        } else {
            if (value_ == std::numeric_limits<__int128_t>::min() || other.value_ == std::numeric_limits<__int128_t>::min()) {
                throw std::out_of_range("BigInt multiplication overflow");
            }
            if (-value_ > std::numeric_limits<__int128_t>::max() / -other.value_) {
                throw std::out_of_range("BigInt multiplication overflow");
            }
        }
    }
    return BigInt(value_ * other.value_);
}

BigInt BigInt::operator/(const BigInt& other) const {
    if (other.value_ == 0) {
        throw std::domain_error("Division by zero");
    }
    // Handling special case: MIN_INT / -1 which overflows
    if (value_ == std::numeric_limits<__int128_t>::min() && other.value_ == -1) {
        throw std::out_of_range("BigInt division overflow");
    }
    return BigInt(value_ / other.value_);
}

BigInt BigInt::operator%(const BigInt& other) const {
    if (other.value_ == 0) {
        throw std::domain_error("Division by zero");
    }
    // Handling special case: MIN_INT % -1 which is mathematically 0, but some architectures fault or overflow
    if (value_ == std::numeric_limits<__int128_t>::min() && other.value_ == -1) {
        return BigInt(0);
    }
    return BigInt(value_ % other.value_);
}

BigInt& BigInt::operator+=(const BigInt& other) {
    *this = *this + other;
    return *this;
}

BigInt& BigInt::operator-=(const BigInt& other) {
    *this = *this - other;
    return *this;
}

BigInt& BigInt::operator*=(const BigInt& other) {
    *this = *this * other;
    return *this;
}

BigInt& BigInt::operator/=(const BigInt& other) {
    *this = *this / other;
    return *this;
}

BigInt& BigInt::operator%=(const BigInt& other) {
    *this = *this % other;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const BigInt& val) {
    os << val.to_string();
    return os;
}

bool BigInt::fits_int64() const {
    return value_ >= std::numeric_limits<int64_t>::min() && value_ <= std::numeric_limits<int64_t>::max();
}

int64_t BigInt::to_int64() const {
    if (!fits_int64()) {
        throw std::out_of_range("BigInt value does not fit in int64_t");
    }
    return static_cast<int64_t>(value_);
}

bool BigInt::fits_uint64() const {
    return value_ >= 0 && value_ <= static_cast<__int128_t>(std::numeric_limits<uint64_t>::max());
}

uint64_t BigInt::to_uint64() const {
    if (!fits_uint64()) {
        throw std::out_of_range("BigInt value does not fit in uint64_t");
    }
    return static_cast<uint64_t>(value_);
}

} // namespace chirp::interpreter
